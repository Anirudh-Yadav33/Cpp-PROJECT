#include "sql_database.h"
#include <iostream>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <algorithm>

static std::string getCurrentTimestamp() {
    std::time_t now = std::time(nullptr);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    return std::string(buf);
}

static std::string sanitizeSQL(const std::string& input) {
    std::string clean;
    for (char c : input) {
        if (c == '\'') {
            clean += "''";
        } else {
            clean += c;
        }
    }
    return clean;
}

SQLDatabase::SQLDatabase(const std::string& path) : db(nullptr), db_path(path) {}

SQLDatabase::~SQLDatabase() {
    if (db) {
        sqlite3_close(db);
        db = nullptr;
    }
}

bool SQLDatabase::executeSQL(const std::string& sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL Error: " << (errMsg ? errMsg : "unknown") << "\nQuery: " << sql << std::endl;
        if (errMsg) sqlite3_free(errMsg);
        return false;
    }
    return true;
}

bool SQLDatabase::init() {
    SimpleLockGuard lock(db_mutex);
    int rc = sqlite3_open(db_path.c_str(), &db);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to open SQLite database: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    std::string sql_users = 
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "username TEXT UNIQUE NOT NULL, "
        "password TEXT NOT NULL, "
        "full_name TEXT NOT NULL, "
        "role TEXT NOT NULL, "
        "created_at TEXT);";

    std::string sql_courses = 
        "CREATE TABLE IF NOT EXISTS courses ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "subject_name TEXT NOT NULL, "
        "faculty_username TEXT NOT NULL, "
        "faculty_name TEXT NOT NULL, "
        "code TEXT UNIQUE NOT NULL, "
        "created_at TEXT);";

    std::string sql_enrollments = 
        "CREATE TABLE IF NOT EXISTS enrollments ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "course_id INTEGER NOT NULL, "
        "student_username TEXT NOT NULL, "
        "student_name TEXT NOT NULL, "
        "status TEXT NOT NULL DEFAULT 'pending', "
        "requested_at TEXT, "
        "UNIQUE(course_id, student_username));";

    std::string sql_submissions = 
        "CREATE TABLE IF NOT EXISTS submissions ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "course_id INTEGER NOT NULL, "
        "student_username TEXT NOT NULL, "
        "student_name TEXT NOT NULL, "
        "title TEXT NOT NULL, "
        "description TEXT NOT NULL, "
        "pdf_filename TEXT NOT NULL, "
        "pdf_content TEXT NOT NULL, "
        "special_code TEXT NOT NULL, "
        "status TEXT NOT NULL DEFAULT 'submitted', "
        "submitted_at TEXT);";

    std::string sql_evaluations = 
        "CREATE TABLE IF NOT EXISTS evaluations ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "submission_id INTEGER NOT NULL, "
        "evaluator_type TEXT NOT NULL, "
        "rubric1_score INTEGER DEFAULT 0, "
        "rubric1_feedback TEXT, "
        "rubric2_score INTEGER DEFAULT 0, "
        "rubric2_feedback TEXT, "
        "rubric3_score INTEGER DEFAULT 0, "
        "rubric3_feedback TEXT, "
        "rubric4_score INTEGER DEFAULT 0, "
        "rubric4_feedback TEXT, "
        "total_score INTEGER DEFAULT 0, "
        "is_pass INTEGER DEFAULT 0, "
        "evaluator_notes TEXT, "
        "evaluated_at TEXT, "
        "UNIQUE(submission_id, evaluator_type));";

    std::string sql_chat = 
        "CREATE TABLE IF NOT EXISTS chat_messages ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "submission_id INTEGER NOT NULL, "
        "sender TEXT NOT NULL, "
        "message TEXT NOT NULL, "
        "suggested_r1 INTEGER DEFAULT 0, "
        "suggested_r2 INTEGER DEFAULT 0, "
        "suggested_r3 INTEGER DEFAULT 0, "
        "suggested_r4 INTEGER DEFAULT 0, "
        "timestamp TEXT);";

    executeSQL(sql_users);
    executeSQL(sql_courses);
    executeSQL(sql_enrollments);
    executeSQL(sql_submissions);
    executeSQL(sql_evaluations);
    executeSQL(sql_chat);

    seedInitialData();
    return true;
}

void SQLDatabase::seedInitialData() {
    User checkUser;
    if (!getUser("prof_smith", checkUser)) {
        User f1 = {0, "prof_smith", "pass123", "Dr. Alan Smith", "faculty", getCurrentTimestamp()};
        createUser(f1);

        User s1 = {0, "alex_student", "pass123", "Alex Johnson", "student", getCurrentTimestamp()};
        User s2 = {0, "sarah_connor", "pass123", "Sarah Connor", "student", getCurrentTimestamp()};
        User s3 = {0, "david_miller", "pass123", "David Miller", "student", getCurrentTimestamp()};
        createUser(s1);
        createUser(s2);
        createUser(s3);

        Course c1 = {0, "Advanced AI & Machine Learning", "prof_smith", "Dr. Alan Smith", "84920", getCurrentTimestamp()};
        Course c2 = {0, "Web Systems & Cloud Computing", "prof_smith", "Dr. Alan Smith", "12345", getCurrentTimestamp()};
        createCourse(c1);
        createCourse(c2);

        requestEnrollment(c1.id, "alex_student", "Alex Johnson");
        requestEnrollment(c1.id, "sarah_connor", "Sarah Connor");
        requestEnrollment(c2.id, "david_miller", "David Miller");

        // Accept Alex and Sarah
        std::vector<Enrollment> pending = getPendingEnrollments("prof_smith");
        for (auto& en : pending) {
            if (en.student_username == "alex_student" || en.student_username == "sarah_connor") {
                updateEnrollmentStatus(en.id, "accepted");
            }
        }
    }
}

bool SQLDatabase::createUser(const User& user) {
    SimpleLockGuard lock(db_mutex);
    std::string sql = "INSERT INTO users (username, password, full_name, role, created_at) VALUES ('" +
        sanitizeSQL(user.username) + "', '" +
        sanitizeSQL(user.password) + "', '" +
        sanitizeSQL(user.full_name) + "', '" +
        sanitizeSQL(user.role) + "', '" +
        getCurrentTimestamp() + "');";
    return executeSQL(sql);
}

bool SQLDatabase::authenticateUser(const std::string& username, const std::string& password, User& out_user) {
    SimpleLockGuard lock(db_mutex);
    std::string sql = "SELECT id, username, password, full_name, role, created_at FROM users WHERE username='" +
        sanitizeSQL(username) + "' AND password='" + sanitizeSQL(password) + "';";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        out_user.id = sqlite3_column_int(stmt, 0);
        out_user.username = (const char*)sqlite3_column_text(stmt, 1);
        out_user.password = (const char*)sqlite3_column_text(stmt, 2);
        out_user.full_name = (const char*)sqlite3_column_text(stmt, 3);
        out_user.role = (const char*)sqlite3_column_text(stmt, 4);
        out_user.created_at = (const char*)sqlite3_column_text(stmt, 5);
        found = true;
    }
    sqlite3_finalize(stmt);
    return found;
}

bool SQLDatabase::getUser(const std::string& username, User& out_user) {
    SimpleLockGuard lock(db_mutex);
    std::string sql = "SELECT id, username, password, full_name, role, created_at FROM users WHERE username='" +
        sanitizeSQL(username) + "';";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        out_user.id = sqlite3_column_int(stmt, 0);
        out_user.username = (const char*)sqlite3_column_text(stmt, 1);
        out_user.password = (const char*)sqlite3_column_text(stmt, 2);
        out_user.full_name = (const char*)sqlite3_column_text(stmt, 3);
        out_user.role = (const char*)sqlite3_column_text(stmt, 4);
        out_user.created_at = (const char*)sqlite3_column_text(stmt, 5);
        found = true;
    }
    sqlite3_finalize(stmt);
    return found;
}

std::vector<User> SQLDatabase::getAllUsers() {
    SimpleLockGuard lock(db_mutex);
    std::vector<User> list;
    std::string sql = "SELECT id, username, password, full_name, role, created_at FROM users;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            User u;
            u.id = sqlite3_column_int(stmt, 0);
            u.username = (const char*)sqlite3_column_text(stmt, 1);
            u.password = (const char*)sqlite3_column_text(stmt, 2);
            u.full_name = (const char*)sqlite3_column_text(stmt, 3);
            u.role = (const char*)sqlite3_column_text(stmt, 4);
            u.created_at = (const char*)sqlite3_column_text(stmt, 5);
            list.push_back(u);
        }
        sqlite3_finalize(stmt);
    }
    return list;
}

bool SQLDatabase::createCourse(Course& course) {
    SimpleLockGuard lock(db_mutex);
    std::string ts = getCurrentTimestamp();
    std::string sql = "INSERT INTO courses (subject_name, faculty_username, faculty_name, code, created_at) VALUES ('" +
        sanitizeSQL(course.subject_name) + "', '" +
        sanitizeSQL(course.faculty_username) + "', '" +
        sanitizeSQL(course.faculty_name) + "', '" +
        sanitizeSQL(course.code) + "', '" +
        ts + "');";

    if (executeSQL(sql)) {
        course.id = (int)sqlite3_last_insert_rowid(db);
        course.created_at = ts;
        return true;
    }
    return false;
}

std::vector<Course> SQLDatabase::getCoursesByFaculty(const std::string& faculty_username) {
    SimpleLockGuard lock(db_mutex);
    std::vector<Course> list;
    std::string sql = "SELECT id, subject_name, faculty_username, faculty_name, code, created_at FROM courses WHERE faculty_username='" +
        sanitizeSQL(faculty_username) + "';";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Course c;
            c.id = sqlite3_column_int(stmt, 0);
            c.subject_name = (const char*)sqlite3_column_text(stmt, 1);
            c.faculty_username = (const char*)sqlite3_column_text(stmt, 2);
            c.faculty_name = (const char*)sqlite3_column_text(stmt, 3);
            c.code = (const char*)sqlite3_column_text(stmt, 4);
            c.created_at = (const char*)sqlite3_column_text(stmt, 5);
            list.push_back(c);
        }
        sqlite3_finalize(stmt);
    }
    return list;
}

std::vector<Course> SQLDatabase::getAllCourses() {
    SimpleLockGuard lock(db_mutex);
    std::vector<Course> list;
    std::string sql = "SELECT id, subject_name, faculty_username, faculty_name, code, created_at FROM courses;";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Course c;
            c.id = sqlite3_column_int(stmt, 0);
            c.subject_name = (const char*)sqlite3_column_text(stmt, 1);
            c.faculty_username = (const char*)sqlite3_column_text(stmt, 2);
            c.faculty_name = (const char*)sqlite3_column_text(stmt, 3);
            c.code = (const char*)sqlite3_column_text(stmt, 4);
            c.created_at = (const char*)sqlite3_column_text(stmt, 5);
            list.push_back(c);
        }
        sqlite3_finalize(stmt);
    }
    return list;
}

bool SQLDatabase::getCourseByCode(const std::string& code, Course& out_course) {
    SimpleLockGuard lock(db_mutex);
    std::string sql = "SELECT id, subject_name, faculty_username, faculty_name, code, created_at FROM courses WHERE code='" +
        sanitizeSQL(code) + "';";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        out_course.id = sqlite3_column_int(stmt, 0);
        out_course.subject_name = (const char*)sqlite3_column_text(stmt, 1);
        out_course.faculty_username = (const char*)sqlite3_column_text(stmt, 2);
        out_course.faculty_name = (const char*)sqlite3_column_text(stmt, 3);
        out_course.code = (const char*)sqlite3_column_text(stmt, 4);
        out_course.created_at = (const char*)sqlite3_column_text(stmt, 5);
        found = true;
    }
    sqlite3_finalize(stmt);
    return found;
}

bool SQLDatabase::requestEnrollment(int course_id, const std::string& student_username, const std::string& student_name) {
    SimpleLockGuard lock(db_mutex);
    std::string sql = "INSERT INTO enrollments (course_id, student_username, student_name, status, requested_at) VALUES (" +
        std::to_string(course_id) + ", '" +
        sanitizeSQL(student_username) + "', '" +
        sanitizeSQL(student_name) + "', 'pending', '" +
        getCurrentTimestamp() + "');";
    return executeSQL(sql);
}

std::vector<Enrollment> SQLDatabase::getPendingEnrollments(const std::string& faculty_username) {
    SimpleLockGuard lock(db_mutex);
    std::vector<Enrollment> list;
    std::string sql = 
        "SELECT e.id, e.course_id, e.student_username, e.student_name, e.status, c.subject_name, c.faculty_name, c.code, e.requested_at "
        "FROM enrollments e JOIN courses c ON e.course_id = c.id "
        "WHERE c.faculty_username='" + sanitizeSQL(faculty_username) + "' AND e.status='pending';";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Enrollment en;
            en.id = sqlite3_column_int(stmt, 0);
            en.course_id = sqlite3_column_int(stmt, 1);
            en.student_username = (const char*)sqlite3_column_text(stmt, 2);
            en.student_name = (const char*)sqlite3_column_text(stmt, 3);
            en.status = (const char*)sqlite3_column_text(stmt, 4);
            en.subject_name = (const char*)sqlite3_column_text(stmt, 5);
            en.faculty_name = (const char*)sqlite3_column_text(stmt, 6);
            en.course_code = (const char*)sqlite3_column_text(stmt, 7);
            en.requested_at = (const char*)sqlite3_column_text(stmt, 8);
            list.push_back(en);
        }
        sqlite3_finalize(stmt);
    }
    return list;
}

std::vector<Enrollment> SQLDatabase::getEnrolledStudents(int course_id) {
    SimpleLockGuard lock(db_mutex);
    std::vector<Enrollment> list;
    std::string sql = 
        "SELECT e.id, e.course_id, e.student_username, e.student_name, e.status, c.subject_name, c.faculty_name, c.code, e.requested_at "
        "FROM enrollments e JOIN courses c ON e.course_id = c.id "
        "WHERE e.course_id=" + std::to_string(course_id) + " AND e.status='accepted';";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Enrollment en;
            en.id = sqlite3_column_int(stmt, 0);
            en.course_id = sqlite3_column_int(stmt, 1);
            en.student_username = (const char*)sqlite3_column_text(stmt, 2);
            en.student_name = (const char*)sqlite3_column_text(stmt, 3);
            en.status = (const char*)sqlite3_column_text(stmt, 4);
            en.subject_name = (const char*)sqlite3_column_text(stmt, 5);
            en.faculty_name = (const char*)sqlite3_column_text(stmt, 6);
            en.course_code = (const char*)sqlite3_column_text(stmt, 7);
            en.requested_at = (const char*)sqlite3_column_text(stmt, 8);
            list.push_back(en);
        }
        sqlite3_finalize(stmt);
    }
    return list;
}

std::vector<Enrollment> SQLDatabase::getStudentEnrollments(const std::string& student_username) {
    SimpleLockGuard lock(db_mutex);
    std::vector<Enrollment> list;
    std::string sql = 
        "SELECT e.id, e.course_id, e.student_username, e.student_name, e.status, c.subject_name, c.faculty_name, c.code, e.requested_at "
        "FROM enrollments e JOIN courses c ON e.course_id = c.id "
        "WHERE e.student_username='" + sanitizeSQL(student_username) + "';";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Enrollment en;
            en.id = sqlite3_column_int(stmt, 0);
            en.course_id = sqlite3_column_int(stmt, 1);
            en.student_username = (const char*)sqlite3_column_text(stmt, 2);
            en.student_name = (const char*)sqlite3_column_text(stmt, 3);
            en.status = (const char*)sqlite3_column_text(stmt, 4);
            en.subject_name = (const char*)sqlite3_column_text(stmt, 5);
            en.faculty_name = (const char*)sqlite3_column_text(stmt, 6);
            en.course_code = (const char*)sqlite3_column_text(stmt, 7);
            en.requested_at = (const char*)sqlite3_column_text(stmt, 8);
            list.push_back(en);
        }
        sqlite3_finalize(stmt);
    }
    return list;
}

bool SQLDatabase::updateEnrollmentStatus(int enrollment_id, const std::string& status) {
    SimpleLockGuard lock(db_mutex);
    std::string sql = "UPDATE enrollments SET status='" + sanitizeSQL(status) + "' WHERE id=" + std::to_string(enrollment_id) + ";";
    return executeSQL(sql);
}

bool SQLDatabase::createSubmission(ProjectSubmission& sub) {
    SimpleLockGuard lock(db_mutex);
    std::string ts = getCurrentTimestamp();
    std::string sql = "INSERT INTO submissions (course_id, student_username, student_name, title, description, pdf_filename, pdf_content, special_code, status, submitted_at) VALUES (" +
        std::to_string(sub.course_id) + ", '" +
        sanitizeSQL(sub.student_username) + "', '" +
        sanitizeSQL(sub.student_name) + "', '" +
        sanitizeSQL(sub.title) + "', '" +
        sanitizeSQL(sub.description) + "', '" +
        sanitizeSQL(sub.pdf_filename) + "', '" +
        sanitizeSQL(sub.pdf_content) + "', '" +
        sanitizeSQL(sub.special_code) + "', 'submitted', '" +
        ts + "');";

    if (executeSQL(sql)) {
        sub.id = (int)sqlite3_last_insert_rowid(db);
        sub.submitted_at = ts;
        sub.status = "submitted";
        return true;
    }
    return false;
}

static void attachEvaluations(sqlite3* db, ProjectSubmission& sub) {
    std::string sql = "SELECT evaluator_type, rubric1_score, rubric1_feedback, rubric2_score, rubric2_feedback, "
                      "rubric3_score, rubric3_feedback, rubric4_score, rubric4_feedback, total_score, is_pass, evaluator_notes, evaluated_at "
                      "FROM evaluations WHERE submission_id=" + std::to_string(sub.id) + ";";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            EvaluationRecord ev;
            ev.submission_id = sub.id;
            ev.evaluator_type = (const char*)sqlite3_column_text(stmt, 0);
            ev.rubric1_score = sqlite3_column_int(stmt, 1);
            ev.rubric1_feedback = (const char*)sqlite3_column_text(stmt, 2);
            ev.rubric2_score = sqlite3_column_int(stmt, 3);
            ev.rubric2_feedback = (const char*)sqlite3_column_text(stmt, 4);
            ev.rubric3_score = sqlite3_column_int(stmt, 5);
            ev.rubric3_feedback = (const char*)sqlite3_column_text(stmt, 6);
            ev.rubric4_score = sqlite3_column_int(stmt, 7);
            ev.rubric4_feedback = (const char*)sqlite3_column_text(stmt, 8);
            ev.total_score = sqlite3_column_int(stmt, 9);
            ev.is_pass = sqlite3_column_int(stmt, 10) != 0;
            ev.evaluator_notes = (const char*)sqlite3_column_text(stmt, 11);
            ev.evaluated_at = (const char*)sqlite3_column_text(stmt, 12);

            if (ev.evaluator_type == "AI") {
                sub.ai_eval = ev;
                sub.has_ai_eval = true;
            } else if (ev.evaluator_type == "FACULTY") {
                sub.faculty_eval = ev;
                sub.has_faculty_eval = true;
            } else if (ev.evaluator_type == "FINAL") {
                sub.final_eval = ev;
                sub.has_final_eval = true;
            }
        }
        sqlite3_finalize(stmt);
    }
}

std::vector<ProjectSubmission> SQLDatabase::getSubmissionsByFaculty(const std::string& faculty_username) {
    SimpleLockGuard lock(db_mutex);
    std::vector<ProjectSubmission> list;
    std::string sql = 
        "SELECT s.id, s.course_id, s.student_username, s.student_name, s.title, s.description, s.pdf_filename, s.pdf_content, s.special_code, s.status, s.submitted_at, c.subject_name, c.faculty_name "
        "FROM submissions s JOIN courses c ON s.course_id = c.id "
        "WHERE c.faculty_username='" + sanitizeSQL(faculty_username) + "' ORDER BY s.id DESC;";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ProjectSubmission sub;
            sub.id = sqlite3_column_int(stmt, 0);
            sub.course_id = sqlite3_column_int(stmt, 1);
            sub.student_username = (const char*)sqlite3_column_text(stmt, 2);
            sub.student_name = (const char*)sqlite3_column_text(stmt, 3);
            sub.title = (const char*)sqlite3_column_text(stmt, 4);
            sub.description = (const char*)sqlite3_column_text(stmt, 5);
            sub.pdf_filename = (const char*)sqlite3_column_text(stmt, 6);
            sub.pdf_content = (const char*)sqlite3_column_text(stmt, 7);
            sub.special_code = (const char*)sqlite3_column_text(stmt, 8);
            sub.status = (const char*)sqlite3_column_text(stmt, 9);
            sub.submitted_at = (const char*)sqlite3_column_text(stmt, 10);
            sub.subject_name = (const char*)sqlite3_column_text(stmt, 11);
            sub.faculty_name = (const char*)sqlite3_column_text(stmt, 12);

            attachEvaluations(db, sub);
            list.push_back(sub);
        }
        sqlite3_finalize(stmt);
    }
    return list;
}

std::vector<ProjectSubmission> SQLDatabase::getSubmissionsByStudent(const std::string& student_username) {
    SimpleLockGuard lock(db_mutex);
    std::vector<ProjectSubmission> list;
    std::string sql = 
        "SELECT s.id, s.course_id, s.student_username, s.student_name, s.title, s.description, s.pdf_filename, s.pdf_content, s.special_code, s.status, s.submitted_at, c.subject_name, c.faculty_name "
        "FROM submissions s JOIN courses c ON s.course_id = c.id "
        "WHERE s.student_username='" + sanitizeSQL(student_username) + "' ORDER BY s.id DESC;";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ProjectSubmission sub;
            sub.id = sqlite3_column_int(stmt, 0);
            sub.course_id = sqlite3_column_int(stmt, 1);
            sub.student_username = (const char*)sqlite3_column_text(stmt, 2);
            sub.student_name = (const char*)sqlite3_column_text(stmt, 3);
            sub.title = (const char*)sqlite3_column_text(stmt, 4);
            sub.description = (const char*)sqlite3_column_text(stmt, 5);
            sub.pdf_filename = (const char*)sqlite3_column_text(stmt, 6);
            sub.pdf_content = (const char*)sqlite3_column_text(stmt, 7);
            sub.special_code = (const char*)sqlite3_column_text(stmt, 8);
            sub.status = (const char*)sqlite3_column_text(stmt, 9);
            sub.submitted_at = (const char*)sqlite3_column_text(stmt, 10);
            sub.subject_name = (const char*)sqlite3_column_text(stmt, 11);
            sub.faculty_name = (const char*)sqlite3_column_text(stmt, 12);

            attachEvaluations(db, sub);
            list.push_back(sub);
        }
        sqlite3_finalize(stmt);
    }
    return list;
}

bool SQLDatabase::getSubmissionById(int id, ProjectSubmission& out_sub) {
    SimpleLockGuard lock(db_mutex);
    std::string sql = 
        "SELECT s.id, s.course_id, s.student_username, s.student_name, s.title, s.description, s.pdf_filename, s.pdf_content, s.special_code, s.status, s.submitted_at, c.subject_name, c.faculty_name "
        "FROM submissions s JOIN courses c ON s.course_id = c.id "
        "WHERE s.id=" + std::to_string(id) + ";";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        out_sub.id = sqlite3_column_int(stmt, 0);
        out_sub.course_id = sqlite3_column_int(stmt, 1);
        out_sub.student_username = (const char*)sqlite3_column_text(stmt, 2);
        out_sub.student_name = (const char*)sqlite3_column_text(stmt, 3);
        out_sub.title = (const char*)sqlite3_column_text(stmt, 4);
        out_sub.description = (const char*)sqlite3_column_text(stmt, 5);
        out_sub.pdf_filename = (const char*)sqlite3_column_text(stmt, 6);
        out_sub.pdf_content = (const char*)sqlite3_column_text(stmt, 7);
        out_sub.special_code = (const char*)sqlite3_column_text(stmt, 8);
        out_sub.status = (const char*)sqlite3_column_text(stmt, 9);
        out_sub.submitted_at = (const char*)sqlite3_column_text(stmt, 10);
        out_sub.subject_name = (const char*)sqlite3_column_text(stmt, 11);
        out_sub.faculty_name = (const char*)sqlite3_column_text(stmt, 12);

        attachEvaluations(db, out_sub);
        found = true;
    }
    sqlite3_finalize(stmt);
    return found;
}

bool SQLDatabase::saveEvaluation(const EvaluationRecord& eval) {
    SimpleLockGuard lock(db_mutex);
    std::string ts = getCurrentTimestamp();
    std::string sql = 
        "INSERT OR REPLACE INTO evaluations (submission_id, evaluator_type, rubric1_score, rubric1_feedback, rubric2_score, rubric2_feedback, "
        "rubric3_score, rubric3_feedback, rubric4_score, rubric4_feedback, total_score, is_pass, evaluator_notes, evaluated_at) VALUES (" +
        std::to_string(eval.submission_id) + ", '" +
        sanitizeSQL(eval.evaluator_type) + "', " +
        std::to_string(eval.rubric1_score) + ", '" +
        sanitizeSQL(eval.rubric1_feedback) + "', " +
        std::to_string(eval.rubric2_score) + ", '" +
        sanitizeSQL(eval.rubric2_feedback) + "', " +
        std::to_string(eval.rubric3_score) + ", '" +
        sanitizeSQL(eval.rubric3_feedback) + "', " +
        std::to_string(eval.rubric4_score) + ", '" +
        sanitizeSQL(eval.rubric4_feedback) + "', " +
        std::to_string(eval.total_score) + ", " +
        std::to_string(eval.is_pass ? 1 : 0) + ", '" +
        sanitizeSQL(eval.evaluator_notes) + "', '" +
        ts + "');";

    if (executeSQL(sql)) {
        if (eval.evaluator_type == "FINAL") {
            std::string update_sub = "UPDATE submissions SET status='graded' WHERE id=" + std::to_string(eval.submission_id) + ";";
            executeSQL(update_sub);
        }
        return true;
    }
    return false;
}

bool SQLDatabase::getEvaluation(int submission_id, const std::string& evaluator_type, EvaluationRecord& out_eval) {
    SimpleLockGuard lock(db_mutex);
    std::string sql = "SELECT rubric1_score, rubric1_feedback, rubric2_score, rubric2_feedback, rubric3_score, rubric3_feedback, "
                      "rubric4_score, rubric4_feedback, total_score, is_pass, evaluator_notes, evaluated_at "
                      "FROM evaluations WHERE submission_id=" + std::to_string(submission_id) + " AND evaluator_type='" + sanitizeSQL(evaluator_type) + "';";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        out_eval.submission_id = submission_id;
        out_eval.evaluator_type = evaluator_type;
        out_eval.rubric1_score = sqlite3_column_int(stmt, 0);
        out_eval.rubric1_feedback = (const char*)sqlite3_column_text(stmt, 1);
        out_eval.rubric2_score = sqlite3_column_int(stmt, 2);
        out_eval.rubric2_feedback = (const char*)sqlite3_column_text(stmt, 3);
        out_eval.rubric3_score = sqlite3_column_int(stmt, 4);
        out_eval.rubric3_feedback = (const char*)sqlite3_column_text(stmt, 5);
        out_eval.rubric4_score = sqlite3_column_int(stmt, 6);
        out_eval.rubric4_feedback = (const char*)sqlite3_column_text(stmt, 7);
        out_eval.total_score = sqlite3_column_int(stmt, 8);
        out_eval.is_pass = sqlite3_column_int(stmt, 9) != 0;
        out_eval.evaluator_notes = (const char*)sqlite3_column_text(stmt, 10);
        out_eval.evaluated_at = (const char*)sqlite3_column_text(stmt, 11);
        found = true;
    }
    sqlite3_finalize(stmt);
    return found;
}

bool SQLDatabase::saveChatMessage(const ChatMessage& msg) {
    SimpleLockGuard lock(db_mutex);
    std::string sql = "INSERT INTO chat_messages (submission_id, sender, message, suggested_r1, suggested_r2, suggested_r3, suggested_r4, timestamp) VALUES (" +
        std::to_string(msg.submission_id) + ", '" +
        sanitizeSQL(msg.sender) + "', '" +
        sanitizeSQL(msg.message) + "', " +
        std::to_string(msg.suggested_r1) + ", " +
        std::to_string(msg.suggested_r2) + ", " +
        std::to_string(msg.suggested_r3) + ", " +
        std::to_string(msg.suggested_r4) + ", '" +
        getCurrentTimestamp() + "');";
    return executeSQL(sql);
}

std::vector<ChatMessage> SQLDatabase::getChatHistory(int submission_id) {
    SimpleLockGuard lock(db_mutex);
    std::vector<ChatMessage> list;
    std::string sql = "SELECT id, submission_id, sender, message, suggested_r1, suggested_r2, suggested_r3, suggested_r4, timestamp "
                      "FROM chat_messages WHERE submission_id=" + std::to_string(submission_id) + " ORDER BY id ASC;";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ChatMessage m;
            m.id = sqlite3_column_int(stmt, 0);
            m.submission_id = sqlite3_column_int(stmt, 1);
            m.sender = (const char*)sqlite3_column_text(stmt, 2);
            m.message = (const char*)sqlite3_column_text(stmt, 3);
            m.suggested_r1 = sqlite3_column_int(stmt, 4);
            m.suggested_r2 = sqlite3_column_int(stmt, 5);
            m.suggested_r3 = sqlite3_column_int(stmt, 6);
            m.suggested_r4 = sqlite3_column_int(stmt, 7);
            m.timestamp = (const char*)sqlite3_column_text(stmt, 8);
            list.push_back(m);
        }
        sqlite3_finalize(stmt);
    }
    return list;
}

AnalyticsSummary SQLDatabase::getAnalyticsForFaculty(const std::string& faculty_username) {
    SimpleLockGuard lock(db_mutex);
    AnalyticsSummary stats = {0, 0, 0, 0, 0.0, 0.0, 0, 100, 0, 0, 0, 0};

    // Total accepted enrolled students for faculty courses
    std::string sql_enrolled = "SELECT COUNT(*) FROM enrollments e JOIN courses c ON e.course_id = c.id "
                               "WHERE c.faculty_username='" + sanitizeSQL(faculty_username) + "' AND e.status='accepted';";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql_enrolled.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) stats.total_enrolled = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }

    // Submissions details
    std::string sql_subs = "SELECT s.id, s.status FROM submissions s JOIN courses c ON s.course_id = c.id "
                           "WHERE c.faculty_username='" + sanitizeSQL(faculty_username) + "';";

    int total_score_sum = 0;
    int graded_scores_count = 0;

    if (sqlite3_prepare_v2(db, sql_subs.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int sub_id = sqlite3_column_int(stmt, 0);
            std::string status = (const char*)sqlite3_column_text(stmt, 1);
            stats.total_submitted++;

            if (status == "graded") {
                stats.graded_count++;
            } else {
                stats.pending_grading++;
            }

            // Check if final evaluation exists
            std::string sql_eval = "SELECT total_score FROM evaluations WHERE submission_id=" + std::to_string(sub_id) + " AND (evaluator_type='FINAL' OR evaluator_type='FACULTY');";
            sqlite3_stmt* stmt_e;
            if (sqlite3_prepare_v2(db, sql_eval.c_str(), -1, &stmt_e, nullptr) == SQLITE_OK) {
                if (sqlite3_step(stmt_e) == SQLITE_ROW) {
                    int score = sqlite3_column_int(stmt_e, 0);
                    total_score_sum += score;
                    graded_scores_count++;

                    if (score > stats.highest_mark) stats.highest_mark = score;
                    if (score < stats.lowest_mark) stats.lowest_mark = score;

                    if (score >= 85) stats.grade_a++;
                    else if (score >= 70) stats.grade_b++;
                    else if (score >= 50) stats.grade_c++;
                    else stats.grade_fail++;
                }
                sqlite3_finalize(stmt_e);
            }
        }
        sqlite3_finalize(stmt);
    }

    if (stats.lowest_mark == 100 && graded_scores_count == 0) stats.lowest_mark = 0;

    if (stats.total_enrolled > 0) {
        stats.submission_ratio = (double)stats.total_submitted / stats.total_enrolled * 100.0;
    } else if (stats.total_submitted > 0) {
        stats.submission_ratio = 100.0;
    }

    if (graded_scores_count > 0) {
        stats.class_average = (double)total_score_sum / graded_scores_count;
    }

    return stats;
}

std::string SQLDatabase::generateCSVExport(const std::string& faculty_username) {
    SimpleLockGuard lock(db_mutex);
    std::stringstream csv;
    csv << "Submission ID,Student Name,Student Username,Subject,Course Code,Project Title,Special Code,Status,Rubric 1 (Novelty/30),Rubric 2 (Code Quality/30),Rubric 3 (Modules & Output/20),Rubric 4 (Doc & Scope/20),Total Score,Pass/Fail,Evaluated At\n";

    std::string sql = 
        "SELECT s.id, s.student_name, s.student_username, c.subject_name, c.code, s.title, s.special_code, s.status "
        "FROM submissions s JOIN courses c ON s.course_id = c.id "
        "WHERE c.faculty_username='" + sanitizeSQL(faculty_username) + "' ORDER BY s.id ASC;";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int sub_id = sqlite3_column_int(stmt, 0);
            std::string student_name = (const char*)sqlite3_column_text(stmt, 1);
            std::string student_username = (const char*)sqlite3_column_text(stmt, 2);
            std::string subject_name = (const char*)sqlite3_column_text(stmt, 3);
            std::string course_code = (const char*)sqlite3_column_text(stmt, 4);
            std::string title = (const char*)sqlite3_column_text(stmt, 5);
            std::string special_code = (const char*)sqlite3_column_text(stmt, 6);
            std::string status = (const char*)sqlite3_column_text(stmt, 7);

            // Fetch final or faculty eval
            int r1 = 0, r2 = 0, r3 = 0, r4 = 0, total = 0;
            std::string is_pass_str = "PENDING";
            std::string eval_at = "-";

            std::string sql_ev = "SELECT rubric1_score, rubric2_score, rubric3_score, rubric4_score, total_score, is_pass, evaluated_at "
                                 "FROM evaluations WHERE submission_id=" + std::to_string(sub_id) + " ORDER BY id DESC LIMIT 1;";
            sqlite3_stmt* stmt_ev;
            if (sqlite3_prepare_v2(db, sql_ev.c_str(), -1, &stmt_ev, nullptr) == SQLITE_OK) {
                if (sqlite3_step(stmt_ev) == SQLITE_ROW) {
                    r1 = sqlite3_column_int(stmt_ev, 0);
                    r2 = sqlite3_column_int(stmt_ev, 1);
                    r3 = sqlite3_column_int(stmt_ev, 2);
                    r4 = sqlite3_column_int(stmt_ev, 3);
                    total = sqlite3_column_int(stmt_ev, 4);
                    int pass = sqlite3_column_int(stmt_ev, 5);
                    is_pass_str = (pass == 1) ? "PASS" : "FAIL";
                    eval_at = (const char*)sqlite3_column_text(stmt_ev, 6);
                }
                sqlite3_finalize(stmt_ev);
            }

            csv << sub_id << ",\""
                << student_name << "\",\""
                << student_username << "\",\""
                << subject_name << "\",\""
                << course_code << "\",\""
                << title << "\",\""
                << special_code << "\",\""
                << status << "\","
                << r1 << "," << r2 << "," << r3 << "," << r4 << ","
                << total << ",\"" << is_pass_str << "\",\"" << eval_at << "\"\n";
        }
        sqlite3_finalize(stmt);
    }

    return csv.str();
}
