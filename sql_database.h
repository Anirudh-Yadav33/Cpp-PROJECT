#ifndef SQL_DATABASE_H
#define SQL_DATABASE_H

#include <string>
#include <vector>
#include <map>
#include <windows.h>
#include "sqlite3.h"

class SimpleMutex {
private:
    CRITICAL_SECTION cs;
public:
    SimpleMutex() { InitializeCriticalSection(&cs); }
    ~SimpleMutex() { DeleteCriticalSection(&cs); }
    void lock() { EnterCriticalSection(&cs); }
    void unlock() { LeaveCriticalSection(&cs); }
};

class SimpleLockGuard {
private:
    SimpleMutex& m;
public:
    SimpleLockGuard(SimpleMutex& mutex) : m(mutex) { m.lock(); }
    ~SimpleLockGuard() { m.unlock(); }
};

struct User {
    int id;
    std::string username;
    std::string password;
    std::string full_name;
    std::string role; // "student" or "faculty"
    std::string created_at;
};

struct Course {
    int id;
    std::string subject_name;
    std::string faculty_username;
    std::string faculty_name;
    std::string code; // 5-digit code
    std::string created_at;
};

struct Enrollment {
    int id;
    int course_id;
    std::string student_username;
    std::string student_name;
    std::string status; // "pending", "accepted", "rejected"
    std::string subject_name;
    std::string faculty_name;
    std::string course_code;
    std::string requested_at;
};

struct EvaluationRecord {
    int id;
    int submission_id;
    std::string evaluator_type; // "AI", "FACULTY", "FINAL"
    int rubric1_score; // Novelty/Existing check (15-30)
    std::string rubric1_feedback;
    int rubric2_score; // Code quality (15-30)
    std::string rubric2_feedback;
    int rubric3_score; // Modules & output (0-20)
    std::string rubric3_feedback;
    int rubric4_score; // Documentation & scope (0-20)
    std::string rubric4_feedback;
    int total_score; // 0-100
    bool is_pass; // total >= 50
    std::string evaluator_notes;
    std::string evaluated_at;
};

struct ProjectSubmission {
    int id;
    int course_id;
    std::string student_username;
    std::string student_name;
    std::string subject_name;
    std::string faculty_name;
    std::string title;
    std::string description;
    std::string pdf_filename;
    std::string pdf_content; // Full text of PDF
    std::string special_code;
    std::string status; // "submitted", "graded"
    std::string submitted_at;
    
    // Evaluated scores attached if graded
    EvaluationRecord ai_eval;
    EvaluationRecord faculty_eval;
    EvaluationRecord final_eval;
    bool has_ai_eval = false;
    bool has_faculty_eval = false;
    bool has_final_eval = false;
};

struct ChatMessage {
    int id;
    int submission_id;
    std::string sender; // "FACULTY" or "AI"
    std::string message;
    int suggested_r1;
    int suggested_r2;
    int suggested_r3;
    int suggested_r4;
    std::string timestamp;
};

struct AnalyticsSummary {
    int total_enrolled;
    int total_submitted;
    int pending_grading;
    int graded_count;
    double submission_ratio; // percentage
    double class_average;
    int highest_mark;
    int lowest_mark;
    int grade_a; // 85-100
    int grade_b; // 70-84
    int grade_c; // 50-69
    int grade_fail; // <50
};

class SQLDatabase {
private:
    sqlite3* db;
    SimpleMutex db_mutex;
    std::string db_path;

    bool executeSQL(const std::string& sql);
    void seedInitialData();

public:
    SQLDatabase(const std::string& path = "project_eval.db");
    ~SQLDatabase();

    bool init();

    // User Operations
    bool createUser(const User& user);
    bool authenticateUser(const std::string& username, const std::string& password, User& out_user);
    bool getUser(const std::string& username, User& out_user);
    std::vector<User> getAllUsers();

    // Course Operations
    bool createCourse(Course& course);
    std::vector<Course> getCoursesByFaculty(const std::string& faculty_username);
    std::vector<Course> getAllCourses();
    bool getCourseByCode(const std::string& code, Course& out_course);

    // Enrollment Operations
    bool requestEnrollment(int course_id, const std::string& student_username, const std::string& student_name);
    std::vector<Enrollment> getPendingEnrollments(const std::string& faculty_username);
    std::vector<Enrollment> getEnrolledStudents(int course_id);
    std::vector<Enrollment> getStudentEnrollments(const std::string& student_username);
    bool updateEnrollmentStatus(int enrollment_id, const std::string& status);

    // Submission Operations
    bool createSubmission(ProjectSubmission& sub);
    std::vector<ProjectSubmission> getSubmissionsByFaculty(const std::string& faculty_username);
    std::vector<ProjectSubmission> getSubmissionsByStudent(const std::string& student_username);
    bool getSubmissionById(int id, ProjectSubmission& out_sub);

    // Evaluation Operations
    bool saveEvaluation(const EvaluationRecord& eval);
    bool getEvaluation(int submission_id, const std::string& evaluator_type, EvaluationRecord& out_eval);

    // Chat Operations
    bool saveChatMessage(const ChatMessage& msg);
    std::vector<ChatMessage> getChatHistory(int submission_id);

    // Analytics & Export
    AnalyticsSummary getAnalyticsForFaculty(const std::string& faculty_username);
    std::string generateCSVExport(const std::string& faculty_username);
};

#endif
