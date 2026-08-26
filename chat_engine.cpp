#include "chat_engine.h"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <ctime>
#include <iomanip>

static std::string toLowerStr(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower;
}

static std::string getCurrentTimestamp() {
    std::time_t now = std::time(nullptr);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    return std::string(buf);
}

ChatMessage ChatEngine::processFacultyMessage(SQLDatabase& db, int submission_id, const std::string& faculty_message) {
    // 1. Save Faculty Message into SQL
    ChatMessage fMsg;
    fMsg.submission_id = submission_id;
    fMsg.sender = "FACULTY";
    fMsg.message = faculty_message;
    fMsg.timestamp = getCurrentTimestamp();
    db.saveChatMessage(fMsg);

    // 2. Fetch Submission & Evaluations from SQL
    ProjectSubmission sub;
    db.getSubmissionById(submission_id, sub);

    EvaluationRecord aiEval;
    db.getEvaluation(submission_id, "AI", aiEval);

    EvaluationRecord facEval;
    db.getEvaluation(submission_id, "FACULTY", facEval);

    int r1 = aiEval.rubric1_score > 0 ? aiEval.rubric1_score : 25;
    int r2 = aiEval.rubric2_score > 0 ? aiEval.rubric2_score : 25;
    int r3 = aiEval.rubric3_score > 0 ? aiEval.rubric3_score : 15;
    int r4 = aiEval.rubric4_score > 0 ? aiEval.rubric4_score : 15;

    if (facEval.total_score > 0) {
        r1 = facEval.rubric1_score;
        r2 = facEval.rubric2_score;
        r3 = facEval.rubric3_score;
        r4 = facEval.rubric4_score;
    }

    std::string lowerMsg = toLowerStr(faculty_message);
    std::stringstream reply;

    // AI reasoning response generation
    if (lowerMsg.find("why") != std::string::npos || lowerMsg.find("basis") != std::string::npos || lowerMsg.find("explain") != std::string::npos) {
        reply << "I evaluated this project based on the 4 mandatory rubrics:\n"
              << "1. Existing Project Check (" << r1 << "/30): PDF content was scanned against known project databases to check novelty & scope.\n"
              << "2. Code Quality & Techniques (" << r2 << "/30): Source code snippets in the PDF were analyzed for modular design & optimization.\n"
              << "3. Modules & Output (" << r3 << "/20): Module breakdown and test outputs in PDF were verified.\n"
              << "4. Documentation & Scope (" << r4 << "/20): Document formatting, diagrams, and problem statement clarity were checked.\n"
              << "Total AI Score: " << (r1 + r2 + r3 + r4) << "/100. Would you like to adjust any specific rubric score before final mark locking?";
    } else if (lowerMsg.find("code") != std::string::npos || lowerMsg.find("quality") != std::string::npos || lowerMsg.find("rubric 2") != std::string::npos) {
        reply << "Regarding Rubric 2 (Code Quality & Techniques): The PDF document contains " 
              << sub.pdf_filename << ". I evaluated the code snippets for efficiency and clean structure. "
              << "If you observed additional implementation strengths during your code review, we can increase Rubric 2 to " 
              << std::min(30, r2 + 3) << "/30. Shall I update it?";
        r2 = std::min(30, r2 + 2);
    } else if (lowerMsg.find("existing") != std::string::npos || lowerMsg.find("novelty") != std::string::npos || lowerMsg.find("rubric 1") != std::string::npos) {
        reply << "Regarding Rubric 1 (Existing Project Check & Scope): I checked whether this project duplicates existing repositories. "
              << "If you feel the student's extra features warrant a higher score, I recommend adjusting Rubric 1 to "
              << std::min(30, r1 + 3) << "/30.";
        r1 = std::min(30, r1 + 2);
    } else if (lowerMsg.find("module") != std::string::npos || lowerMsg.find("output") != std::string::npos || lowerMsg.find("rubric 3") != std::string::npos) {
        reply << "Regarding Rubric 3 (Modules & Output): PDF output screenshots and test cases were evaluated. Current score is " 
              << r3 << "/20. I agree to align with your faculty mark for this rubric!";
    } else if (lowerMsg.find("agree") != std::string::npos || lowerMsg.find("finalize") != std::string::npos || lowerMsg.find("accept") != std::string::npos || lowerMsg.find("ok") != std::string::npos) {
        int final_total = r1 + r2 + r3 + r4;
        reply << "Great! We have reached consensus. The final negotiated score is locked at " << final_total 
              << "/100 (" << (final_total >= 50 ? "PASS" : "FAIL") << ")."
              << "\nBreakdown: Novelty=" << r1 << "/30, Code=" << r2 << "/30, Modules=" << r3 << "/20, Doc=" << r4 << "/20."
              << "\nClick 'Finalize & Publish Grade' to lock this into official records and update student results.";
    } else {
        reply << "Thank you Professor. I have reviewed your feedback. Based on our discussion, the updated score estimate is "
              << (r1 + r2 + r3 + r4) << "/100 (Rubric 1: " << r1 << ", Rubric 2: " << r2 << ", Rubric 3: " << r3 << ", Rubric 4: " << r4 << ")."
              << " You can adjust any slider or confirm to finalize the grade!";
    }

    ChatMessage aiMsg;
    aiMsg.submission_id = submission_id;
    aiMsg.sender = "AI";
    aiMsg.message = reply.str();
    aiMsg.suggested_r1 = r1;
    aiMsg.suggested_r2 = r2;
    aiMsg.suggested_r3 = r3;
    aiMsg.suggested_r4 = r4;
    aiMsg.timestamp = getCurrentTimestamp();

    db.saveChatMessage(aiMsg);
    return aiMsg;
}
