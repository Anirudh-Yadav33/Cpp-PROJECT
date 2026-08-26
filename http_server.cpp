#include "http_server.h"
#include "ai_evaluator.h"
#include "chat_engine.h"
#include "excel_exporter.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

struct ClientThreadArgs {
    HTTPServer* server;
    SOCKET clientSocket;
};

static DWORD WINAPI ClientThreadProc(LPVOID lpParam) {
    ClientThreadArgs* args = (ClientThreadArgs*)lpParam;
    args->server->handleClient(args->clientSocket);
    delete args;
    return 0;
}

static DWORD WINAPI ListenThreadProc(LPVOID lpParam) {
    HTTPServer* server = (HTTPServer*)lpParam;
    server->runServerLoop();
    return 0;
}

// Simple JSON Helper functions for C++ API string processing
static std::string jsonEscape(const std::string& input) {
    std::string out;
    out.reserve(input.size() * 2);
    for (char c : input) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out;
}

static std::string getJsonValue(const std::string& json, const std::string& key) {
    std::string pattern = "\"" + key + "\":";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return "";

    pos += pattern.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r')) {
        pos++;
    }

    if (pos >= json.length()) return "";

    if (json[pos] == '"') {
        pos++;
        size_t end = json.find('"', pos);
        while (end != std::string::npos && json[end - 1] == '\\') {
            end = json.find('"', end + 1);
        }
        if (end == std::string::npos) return "";
        return json.substr(pos, end - pos);
    } else {
        size_t end = json.find_first_of(",}\n\r\t ", pos);
        if (end == std::string::npos) end = json.length();
        return json.substr(pos, end - pos);
    }
}

static std::string parseUrlQueryParam(const std::string& path, const std::string& key) {
    size_t qpos = path.find('?');
    if (qpos == std::string::npos) return "";
    std::string query = path.substr(qpos + 1);
    std::stringstream ss(query);
    std::string item;
    while (std::getline(ss, item, '&')) {
        size_t eq = item.find('=');
        if (eq != std::string::npos) {
            std::string k = item.substr(0, eq);
            std::string v = item.substr(eq + 1);
            if (k == key) return v;
        }
    }
    return "";
}

static std::string buildHTTPResponse(int statusCode, const std::string& contentType, const std::string& body, const std::string& extraHeaders = "") {
    std::stringstream ss;
    std::string statusStr = (statusCode == 200) ? "200 OK" : (statusCode == 400 ? "400 Bad Request" : (statusCode == 404 ? "404 Not Found" : "500 Internal Error"));
    ss << "HTTP/1.1 " << statusStr << "\r\n";
    ss << "Content-Type: " << contentType << "\r\n";
    ss << "Content-Length: " << body.size() << "\r\n";
    ss << "Access-Control-Allow-Origin: *\r\n";
    ss << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    ss << "Access-Control-Allow-Headers: Content-Type\r\n";
    if (!extraHeaders.empty()) ss << extraHeaders;
    ss << "Connection: close\r\n\r\n";
    ss << body;
    return ss.str();
}

HTTPServer::HTTPServer(int p, SQLDatabase& database) : port(p), listenSocket(INVALID_SOCKET), running(false), db(database) {}

HTTPServer::~HTTPServer() {
    stop();
}

bool HTTPServer::start() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return false;
    }

    listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed\n";
        WSACleanup();
        return false;
    }

    int opt = 1;
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed on port " << port << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return false;
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed\n";
        closesocket(listenSocket);
        WSACleanup();
        return false;
    }

    running = true;
    std::cout << "[C++ Backend Server] Server running natively on http://localhost:" << port << std::endl;

    HANDLE hThread = CreateThread(NULL, 0, ListenThreadProc, this, 0, NULL);
    if (hThread) CloseHandle(hThread);

    return true;
}

void HTTPServer::runServerLoop() {
    while (running) {
        SOCKET clientSocket = accept(listenSocket, nullptr, nullptr);
        if (clientSocket != INVALID_SOCKET) {
            ClientThreadArgs* args = new ClientThreadArgs{ this, clientSocket };
            HANDLE hClient = CreateThread(NULL, 0, ClientThreadProc, args, 0, NULL);
            if (hClient) CloseHandle(hClient);
        }
    }
}

void HTTPServer::stop() {
    if (running) {
        running = false;
        if (listenSocket != INVALID_SOCKET) {
            closesocket(listenSocket);
            listenSocket = INVALID_SOCKET;
        }
        WSACleanup();
    }
}

void HTTPServer::handleClient(SOCKET clientSocket) {
    char buffer[16384];
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesReceived <= 0) {
        closesocket(clientSocket);
        return;
    }
    buffer[bytesReceived] = '\0';
    std::string rawRequest(buffer, bytesReceived);

    std::stringstream ss(rawRequest);
    std::string method, path, httpVer;
    ss >> method >> path >> httpVer;

    std::map<std::string, std::string> headers;
    std::string line;
    std::getline(ss, line); // finish first line
    size_t contentLength = 0;

    while (std::getline(ss, line) && line != "\r" && !line.empty()) {
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string hKey = line.substr(0, colon);
            std::string hVal = line.substr(colon + 1);
            // trim
            hVal.erase(0, hVal.find_first_not_of(" \t\r\n"));
            hVal.erase(hVal.find_last_not_of(" \t\r\n") + 1);
            headers[hKey] = hVal;
            if (hKey == "Content-Length" || hKey == "content-length") {
                contentLength = std::stoul(hVal);
            }
        }
    }

    std::string body;
    size_t bodyPos = rawRequest.find("\r\n\r\n");
    if (bodyPos != std::string::npos) {
        body = rawRequest.substr(bodyPos + 4);
    }

    while (body.length() < contentLength) {
        int r = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (r <= 0) break;
        body.append(buffer, r);
    }

    std::string response = processRequest(method, path, headers, body);
    send(clientSocket, response.c_str(), (int)response.length(), 0);
    closesocket(clientSocket);
}

std::string HTTPServer::getContentType(const std::string& path) {
    if (path.find(".html") != std::string::npos) return "text/html";
    if (path.find(".css") != std::string::npos) return "text/css";
    if (path.find(".js") != std::string::npos) return "application/javascript";
    if (path.find(".json") != std::string::npos) return "application/json";
    if (path.find(".png") != std::string::npos) return "image/png";
    if (path.find(".jpg") != std::string::npos) return "image/jpeg";
    if (path.find(".svg") != std::string::npos) return "image/svg+xml";
    if (path.find(".csv") != std::string::npos) return "text/csv";
    return "text/plain";
}

std::string HTTPServer::serveStaticFile(const std::string& filepath) {
    std::string fullPath = "public" + filepath;
    if (filepath == "/") fullPath = "public/index.html";

    std::ifstream file(fullPath, std::ios::binary);
    if (!file.is_open()) {
        return buildHTTPResponse(404, "text/plain", "404 Not Found");
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buildHTTPResponse(200, getContentType(fullPath), buffer.str());
}

static std::string formatSubmissionJSON(const ProjectSubmission& sub) {
    std::stringstream json;
    json << "{"
         << "\"id\":" << sub.id << ","
         << "\"course_id\":" << sub.course_id << ","
         << "\"student_username\":\"" << jsonEscape(sub.student_username) << "\","
         << "\"student_name\":\"" << jsonEscape(sub.student_name) << "\","
         << "\"subject_name\":\"" << jsonEscape(sub.subject_name) << "\","
         << "\"faculty_name\":\"" << jsonEscape(sub.faculty_name) << "\","
         << "\"title\":\"" << jsonEscape(sub.title) << "\","
         << "\"description\":\"" << jsonEscape(sub.description) << "\","
         << "\"pdf_filename\":\"" << jsonEscape(sub.pdf_filename) << "\","
         << "\"pdf_content\":\"" << jsonEscape(sub.pdf_content) << "\","
         << "\"special_code\":\"" << jsonEscape(sub.special_code) << "\","
         << "\"status\":\"" << jsonEscape(sub.status) << "\","
         << "\"submitted_at\":\"" << jsonEscape(sub.submitted_at) << "\","
         << "\"has_ai_eval\":" << (sub.has_ai_eval ? "true" : "false") << ","
         << "\"has_faculty_eval\":" << (sub.has_faculty_eval ? "true" : "false") << ","
         << "\"has_final_eval\":" << (sub.has_final_eval ? "true" : "false");

    if (sub.has_ai_eval) {
        json << ",\"ai_eval\":{"
             << "\"r1\":" << sub.ai_eval.rubric1_score << ","
             << "\"r1_fb\":\"" << jsonEscape(sub.ai_eval.rubric1_feedback) << "\","
             << "\"r2\":" << sub.ai_eval.rubric2_score << ","
             << "\"r2_fb\":\"" << jsonEscape(sub.ai_eval.rubric2_feedback) << "\","
             << "\"r3\":" << sub.ai_eval.rubric3_score << ","
             << "\"r3_fb\":\"" << jsonEscape(sub.ai_eval.rubric3_feedback) << "\","
             << "\"r4\":" << sub.ai_eval.rubric4_score << ","
             << "\"r4_fb\":\"" << jsonEscape(sub.ai_eval.rubric4_feedback) << "\","
             << "\"total\":" << sub.ai_eval.total_score << ","
             << "\"is_pass\":" << (sub.ai_eval.is_pass ? "true" : "false") << ","
             << "\"notes\":\"" << jsonEscape(sub.ai_eval.evaluator_notes) << "\""
             << "}";
    }

    if (sub.has_faculty_eval) {
        json << ",\"faculty_eval\":{"
             << "\"r1\":" << sub.faculty_eval.rubric1_score << ","
             << "\"r1_fb\":\"" << jsonEscape(sub.faculty_eval.rubric1_feedback) << "\","
             << "\"r2\":" << sub.faculty_eval.rubric2_score << ","
             << "\"r2_fb\":\"" << jsonEscape(sub.faculty_eval.rubric2_feedback) << "\","
             << "\"r3\":" << sub.faculty_eval.rubric3_score << ","
             << "\"r3_fb\":\"" << jsonEscape(sub.faculty_eval.rubric3_feedback) << "\","
             << "\"r4\":" << sub.faculty_eval.rubric4_score << ","
             << "\"r4_fb\":\"" << jsonEscape(sub.faculty_eval.rubric4_feedback) << "\","
             << "\"total\":" << sub.faculty_eval.total_score << ","
             << "\"is_pass\":" << (sub.faculty_eval.is_pass ? "true" : "false") << ","
             << "\"notes\":\"" << jsonEscape(sub.faculty_eval.evaluator_notes) << "\""
             << "}";
    }

    if (sub.has_final_eval) {
        json << ",\"final_eval\":{"
             << "\"r1\":" << sub.final_eval.rubric1_score << ","
             << "\"r1_fb\":\"" << jsonEscape(sub.final_eval.rubric1_feedback) << "\","
             << "\"r2\":" << sub.final_eval.rubric2_score << ","
             << "\"r2_fb\":\"" << jsonEscape(sub.final_eval.rubric2_feedback) << "\","
             << "\"r3\":" << sub.final_eval.rubric3_score << ","
             << "\"r3_fb\":\"" << jsonEscape(sub.final_eval.rubric3_feedback) << "\","
             << "\"r4\":" << sub.final_eval.rubric4_score << ","
             << "\"r4_fb\":\"" << jsonEscape(sub.final_eval.rubric4_feedback) << "\","
             << "\"total\":" << sub.final_eval.total_score << ","
             << "\"is_pass\":" << (sub.final_eval.is_pass ? "true" : "false") << ","
             << "\"notes\":\"" << jsonEscape(sub.final_eval.evaluator_notes) << "\""
             << "}";
    }

    json << "}";
    return json.str();
}

std::string HTTPServer::processRequest(const std::string& method, const std::string& rawPath, const std::map<std::string, std::string>& headers, const std::string& body) {
    if (method == "OPTIONS") {
        return buildHTTPResponse(200, "text/plain", "OK");
    }

    std::string path = rawPath;
    size_t qmark = path.find('?');
    std::string cleanPath = (qmark != std::string::npos) ? path.substr(0, qmark) : path;

    // ----------------------------------------------------
    // API Endpoints
    // ----------------------------------------------------
    if (cleanPath == "/api/login" && method == "POST") {
        std::string username = getJsonValue(body, "username");
        std::string password = getJsonValue(body, "password");
        User user;
        if (db.authenticateUser(username, password, user)) {
            std::stringstream resp;
            resp << "{\"success\":true,\"user\":{"
                 << "\"id\":" << user.id << ","
                 << "\"username\":\"" << jsonEscape(user.username) << "\","
                 << "\"full_name\":\"" << jsonEscape(user.full_name) << "\","
                 << "\"role\":\"" << jsonEscape(user.role) << "\""
                 << "}}";
            return buildHTTPResponse(200, "application/json", resp.str());
        }
        return buildHTTPResponse(400, "application/json", "{\"success\":false,\"message\":\"Invalid username or password\"}");
    }

    if (cleanPath == "/api/register" && method == "POST") {
        User u;
        u.username = getJsonValue(body, "username");
        u.password = getJsonValue(body, "password");
        u.full_name = getJsonValue(body, "full_name");
        u.role = getJsonValue(body, "role");

        if (u.username.empty() || u.password.empty() || u.full_name.empty()) {
            return buildHTTPResponse(400, "application/json", "{\"success\":false,\"message\":\"Missing required fields\"}");
        }

        if (db.createUser(u)) {
            return buildHTTPResponse(200, "application/json", "{\"success\":true,\"message\":\"Account registered successfully!\"}");
        }
        return buildHTTPResponse(400, "application/json", "{\"success\":false,\"message\":\"Username already exists\"}");
    }

    if (cleanPath == "/api/courses/list" && method == "GET") {
        std::string faculty = parseUrlQueryParam(rawPath, "faculty");
        std::vector<Course> courses = faculty.empty() ? db.getAllCourses() : db.getCoursesByFaculty(faculty);

        std::stringstream json;
        json << "{\"success\":true,\"courses\":[";
        for (size_t i = 0; i < courses.size(); ++i) {
            json << "{"
                 << "\"id\":" << courses[i].id << ","
                 << "\"subject_name\":\"" << jsonEscape(courses[i].subject_name) << "\","
                 << "\"faculty_username\":\"" << jsonEscape(courses[i].faculty_username) << "\","
                 << "\"faculty_name\":\"" << jsonEscape(courses[i].faculty_name) << "\","
                 << "\"code\":\"" << jsonEscape(courses[i].code) << "\","
                 << "\"created_at\":\"" << jsonEscape(courses[i].created_at) << "\""
                 << "}" << (i + 1 < courses.size() ? "," : "");
        }
        json << "]}";
        return buildHTTPResponse(200, "application/json", json.str());
    }

    if (cleanPath == "/api/courses/create" && method == "POST") {
        Course c;
        c.subject_name = getJsonValue(body, "subject_name");
        c.faculty_username = getJsonValue(body, "faculty_username");
        c.faculty_name = getJsonValue(body, "faculty_name");
        c.code = getJsonValue(body, "code");

        if (c.subject_name.empty() || c.code.length() != 5) {
            return buildHTTPResponse(400, "application/json", "{\"success\":false,\"message\":\"Subject name required & Code must be 5 digits\"}");
        }

        if (db.createCourse(c)) {
            std::stringstream ss;
            ss << "{\"success\":true,\"message\":\"Course created successfully!\",\"course_id\":" << c.id << "}";
            return buildHTTPResponse(200, "application/json", ss.str());
        }
        return buildHTTPResponse(400, "application/json", "{\"success\":false,\"message\":\"5-digit code already in use\"}");
    }

    if (cleanPath == "/api/courses/enroll" && method == "POST") {
        std::string code = getJsonValue(body, "code");
        std::string student_username = getJsonValue(body, "student_username");
        std::string student_name = getJsonValue(body, "student_name");

        Course c;
        if (!db.getCourseByCode(code, c)) {
            return buildHTTPResponse(400, "application/json", "{\"success\":false,\"message\":\"Invalid 5-digit course code\"}");
        }

        if (db.requestEnrollment(c.id, student_username, student_name)) {
            return buildHTTPResponse(200, "application/json", "{\"success\":true,\"message\":\"Enrollment request sent to faculty! Status: Pending Approval\"}");
        }
        return buildHTTPResponse(400, "application/json", "{\"success\":false,\"message\":\"Already requested or enrolled in this course\"}");
    }

    if (cleanPath == "/api/courses/requests" && method == "GET") {
        std::string faculty = parseUrlQueryParam(rawPath, "faculty");
        std::string student = parseUrlQueryParam(rawPath, "student");

        std::stringstream json;
        json << "{\"success\":true,\"enrollments\":[";

        if (!faculty.empty()) {
            std::vector<Enrollment> list = db.getPendingEnrollments(faculty);
            for (size_t i = 0; i < list.size(); ++i) {
                json << "{"
                     << "\"id\":" << list[i].id << ","
                     << "\"course_id\":" << list[i].course_id << ","
                     << "\"student_username\":\"" << jsonEscape(list[i].student_username) << "\","
                     << "\"student_name\":\"" << jsonEscape(list[i].student_name) << "\","
                     << "\"subject_name\":\"" << jsonEscape(list[i].subject_name) << "\","
                     << "\"faculty_name\":\"" << jsonEscape(list[i].faculty_name) << "\","
                     << "\"course_code\":\"" << jsonEscape(list[i].course_code) << "\","
                     << "\"status\":\"" << jsonEscape(list[i].status) << "\","
                     << "\"requested_at\":\"" << jsonEscape(list[i].requested_at) << "\""
                     << "}" << (i + 1 < list.size() ? "," : "");
            }
        } else if (!student.empty()) {
            std::vector<Enrollment> list = db.getStudentEnrollments(student);
            for (size_t i = 0; i < list.size(); ++i) {
                json << "{"
                     << "\"id\":" << list[i].id << ","
                     << "\"course_id\":" << list[i].course_id << ","
                     << "\"student_username\":\"" << jsonEscape(list[i].student_username) << "\","
                     << "\"student_name\":\"" << jsonEscape(list[i].student_name) << "\","
                     << "\"subject_name\":\"" << jsonEscape(list[i].subject_name) << "\","
                     << "\"faculty_name\":\"" << jsonEscape(list[i].faculty_name) << "\","
                     << "\"course_code\":\"" << jsonEscape(list[i].course_code) << "\","
                     << "\"status\":\"" << jsonEscape(list[i].status) << "\","
                     << "\"requested_at\":\"" << jsonEscape(list[i].requested_at) << "\""
                     << "}" << (i + 1 < list.size() ? "," : "");
            }
        }
        json << "]}";
        return buildHTTPResponse(200, "application/json", json.str());
    }

    if (cleanPath == "/api/courses/approve" && method == "POST") {
        int enrollment_id = std::stoi(getJsonValue(body, "enrollment_id"));
        std::string status = getJsonValue(body, "status"); // "accepted" or "rejected"

        if (db.updateEnrollmentStatus(enrollment_id, status)) {
            return buildHTTPResponse(200, "application/json", "{\"success\":true,\"message\":\"Enrollment request updated!\"}");
        }
        return buildHTTPResponse(400, "application/json", "{\"success\":false,\"message\":\"Failed to update enrollment\"}");
    }

    if (cleanPath == "/api/submissions/create" && method == "POST") {
        ProjectSubmission sub;
        sub.course_id = std::stoi(getJsonValue(body, "course_id"));
        sub.student_username = getJsonValue(body, "student_username");
        sub.student_name = getJsonValue(body, "student_name");
        sub.title = getJsonValue(body, "title");
        sub.description = getJsonValue(body, "description");
        sub.pdf_filename = getJsonValue(body, "pdf_filename");
        sub.pdf_content = getJsonValue(body, "pdf_content");
        sub.special_code = getJsonValue(body, "special_code");

        if (sub.title.empty() || sub.special_code.length() != 5) {
            return buildHTTPResponse(400, "application/json", "{\"success\":false,\"message\":\"Project title required and Special Code must be 5 digits\"}");
        }

        if (db.createSubmission(sub)) {
            std::stringstream ss;
            ss << "{\"success\":true,\"message\":\"Project submitted successfully to faculty!\",\"submission_id\":" << sub.id << "}";
            return buildHTTPResponse(200, "application/json", ss.str());
        }
        return buildHTTPResponse(400, "application/json", "{\"success\":false,\"message\":\"Failed to submit project\"}");
    }

    if (cleanPath == "/api/submissions/list" && method == "GET") {
        std::string faculty = parseUrlQueryParam(rawPath, "faculty");
        std::string student = parseUrlQueryParam(rawPath, "student");

        std::vector<ProjectSubmission> list;
        if (!faculty.empty()) list = db.getSubmissionsByFaculty(faculty);
        else if (!student.empty()) list = db.getSubmissionsByStudent(student);

        std::stringstream json;
        json << "{\"success\":true,\"submissions\":[";
        for (size_t i = 0; i < list.size(); ++i) {
            json << formatSubmissionJSON(list[i]) << (i + 1 < list.size() ? "," : "");
        }
        json << "]}";
        return buildHTTPResponse(200, "application/json", json.str());
    }

    if (cleanPath == "/api/submissions/detail" && method == "GET") {
        int id = std::stoi(parseUrlQueryParam(rawPath, "id"));
        ProjectSubmission sub;
        if (db.getSubmissionById(id, sub)) {
            std::stringstream json;
            json << "{\"success\":true,\"submission\":" << formatSubmissionJSON(sub) << "}";
            return buildHTTPResponse(200, "application/json", json.str());
        }
        return buildHTTPResponse(404, "application/json", "{\"success\":false,\"message\":\"Submission not found\"}");
    }

    if (cleanPath == "/api/evaluate/ai" && method == "POST") {
        int sub_id = std::stoi(getJsonValue(body, "submission_id"));
        ProjectSubmission sub;
        if (!db.getSubmissionById(sub_id, sub)) {
            return buildHTTPResponse(404, "application/json", "{\"success\":false,\"message\":\"Submission not found\"}");
        }

        EvaluationRecord aiEval = AIEvaluator::evaluateSubmission(sub);
        if (db.saveEvaluation(aiEval)) {
            std::stringstream json;
            json << "{\"success\":true,\"message\":\"AI Evaluation Completed successfully!\",\"evaluation\":{"
                 << "\"r1\":" << aiEval.rubric1_score << ","
                 << "\"r1_fb\":\"" << jsonEscape(aiEval.rubric1_feedback) << "\","
                 << "\"r2\":" << aiEval.rubric2_score << ","
                 << "\"r2_fb\":\"" << jsonEscape(aiEval.rubric2_feedback) << "\","
                 << "\"r3\":" << aiEval.rubric3_score << ","
                 << "\"r3_fb\":\"" << jsonEscape(aiEval.rubric3_feedback) << "\","
                 << "\"r4\":" << aiEval.rubric4_score << ","
                 << "\"r4_fb\":\"" << jsonEscape(aiEval.rubric4_feedback) << "\","
                 << "\"total\":" << aiEval.total_score << ","
                 << "\"is_pass\":" << (aiEval.is_pass ? "true" : "false") << ","
                 << "\"notes\":\"" << jsonEscape(aiEval.evaluator_notes) << "\""
                 << "}}";
            return buildHTTPResponse(200, "application/json", json.str());
        }
        return buildHTTPResponse(400, "application/json", "{\"success\":false,\"message\":\"Failed to save AI evaluation\"}");
    }

    if (cleanPath == "/api/evaluate/faculty" && method == "POST") {
        EvaluationRecord eval;
        eval.submission_id = std::stoi(getJsonValue(body, "submission_id"));
        eval.evaluator_type = getJsonValue(body, "is_final") == "true" ? "FINAL" : "FACULTY";
        eval.rubric1_score = std::stoi(getJsonValue(body, "rubric1_score"));
        eval.rubric1_feedback = getJsonValue(body, "rubric1_feedback");
        eval.rubric2_score = std::stoi(getJsonValue(body, "rubric2_score"));
        eval.rubric2_feedback = getJsonValue(body, "rubric2_feedback");
        eval.rubric3_score = std::stoi(getJsonValue(body, "rubric3_score"));
        eval.rubric3_feedback = getJsonValue(body, "rubric3_feedback");
        eval.rubric4_score = std::stoi(getJsonValue(body, "rubric4_score"));
        eval.rubric4_feedback = getJsonValue(body, "rubric4_feedback");
        eval.total_score = eval.rubric1_score + eval.rubric2_score + eval.rubric3_score + eval.rubric4_score;
        eval.is_pass = (eval.total_score >= 50);
        eval.evaluator_notes = getJsonValue(body, "evaluator_notes");

        if (db.saveEvaluation(eval)) {
            return buildHTTPResponse(200, "application/json", "{\"success\":true,\"message\":\"Evaluation saved & marks updated!\"}");
        }
        return buildHTTPResponse(400, "application/json", "{\"success\":false,\"message\":\"Failed to save evaluation\"}");
    }

    if (cleanPath == "/api/chat/send" && method == "POST") {
        int sub_id = std::stoi(getJsonValue(body, "submission_id"));
        std::string message = getJsonValue(body, "message");

        ChatMessage aiReply = ChatEngine::processFacultyMessage(db, sub_id, message);

        std::stringstream json;
        json << "{\"success\":true,\"reply\":{"
             << "\"id\":" << aiReply.id << ","
             << "\"sender\":\"" << jsonEscape(aiReply.sender) << "\","
             << "\"message\":\"" << jsonEscape(aiReply.message) << "\","
             << "\"suggested_r1\":" << aiReply.suggested_r1 << ","
             << "\"suggested_r2\":" << aiReply.suggested_r2 << ","
             << "\"suggested_r3\":" << aiReply.suggested_r3 << ","
             << "\"suggested_r4\":" << aiReply.suggested_r4 << ","
             << "\"timestamp\":\"" << jsonEscape(aiReply.timestamp) << "\""
             << "}}";
        return buildHTTPResponse(200, "application/json", json.str());
    }

    if (cleanPath == "/api/chat/history" && method == "GET") {
        int sub_id = std::stoi(parseUrlQueryParam(rawPath, "submission_id"));
        std::vector<ChatMessage> history = db.getChatHistory(sub_id);

        std::stringstream json;
        json << "{\"success\":true,\"messages\":[";
        for (size_t i = 0; i < history.size(); ++i) {
            json << "{"
                 << "\"id\":" << history[i].id << ","
                 << "\"sender\":\"" << jsonEscape(history[i].sender) << "\","
                 << "\"message\":\"" << jsonEscape(history[i].message) << "\","
                 << "\"suggested_r1\":" << history[i].suggested_r1 << ","
                 << "\"suggested_r2\":" << history[i].suggested_r2 << ","
                 << "\"suggested_r3\":" << history[i].suggested_r3 << ","
                 << "\"suggested_r4\":" << history[i].suggested_r4 << ","
                 << "\"timestamp\":\"" << jsonEscape(history[i].timestamp) << "\""
                 << "}" << (i + 1 < history.size() ? "," : "");
        }
        json << "]}";
        return buildHTTPResponse(200, "application/json", json.str());
    }

    if (cleanPath == "/api/analytics" && method == "GET") {
        std::string faculty = parseUrlQueryParam(rawPath, "faculty");
        AnalyticsSummary s = db.getAnalyticsForFaculty(faculty);

        std::stringstream json;
        json << "{\"success\":true,\"analytics\":{"
             << "\"total_enrolled\":" << s.total_enrolled << ","
             << "\"total_submitted\":" << s.total_submitted << ","
             << "\"pending_grading\":" << s.pending_grading << ","
             << "\"graded_count\":" << s.graded_count << ","
             << "\"submission_ratio\":" << s.submission_ratio << ","
             << "\"class_average\":" << s.class_average << ","
             << "\"highest_mark\":" << s.highest_mark << ","
             << "\"lowest_mark\":" << s.lowest_mark << ","
             << "\"grade_a\":" << s.grade_a << ","
             << "\"grade_b\":" << s.grade_b << ","
             << "\"grade_c\":" << s.grade_c << ","
             << "\"grade_fail\":" << s.grade_fail
             << "}}";
        return buildHTTPResponse(200, "application/json", json.str());
    }

    if (cleanPath == "/api/export/excel" && method == "GET") {
        std::string faculty = parseUrlQueryParam(rawPath, "faculty");
        std::string csvData = ExcelExporter::exportFacultyGradesToCSV(db, faculty);
        return buildHTTPResponse(200, "text/csv", csvData, "Content-Disposition: attachment; filename=\"project_evaluations_report.csv\"\r\n");
    }

    // Default static file route
    return serveStaticFile(cleanPath);
}
