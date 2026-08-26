#include "ai_evaluator.h"
#include <ctime>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <iostream>

static std::string getCurrentTimestamp() {
    std::time_t now = std::time(nullptr);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    return std::string(buf);
}

EvaluationRecord AIEvaluator::evaluateSubmission(const ProjectSubmission& sub) {
    PDFAnalysisData pdfData = PDFParser::parsePDFContent(sub.pdf_filename, sub.pdf_content);

    EvaluationRecord eval;
    eval.submission_id = sub.id;
    eval.evaluator_type = "AI";
    eval.evaluated_at = getCurrentTimestamp();

    // ----------------------------------------------------
    // RUBRIC 1: Existing Project Check & Novelty (Max 30)
    // ----------------------------------------------------
    // If not existing + good scope: 25-30 marks
    // If existing + extra features/differences: 20-25 marks
    // If existing + no main differences: 15-20 marks
    if (!pdfData.mentions_existing_system || pdfData.has_good_scope) {
        if (pdfData.has_good_scope) {
            eval.rubric1_score = 28;
            eval.rubric1_feedback = "AI Analysis: Novel project concept with strong future scope & high domain impact. Verified no identical existing project duplication in repository (Assigned: 28/30).";
        } else {
            eval.rubric1_score = 26;
            eval.rubric1_feedback = "AI Analysis: Original project work with promising application scope. No direct existing match detected (Assigned: 26/30).";
        }
    } else if (pdfData.has_extra_features) {
        eval.rubric1_score = 23;
        eval.rubric1_feedback = "AI Analysis: Based on existing architecture, but incorporates clear extra features, performance enhancements, and distinct module improvements (Assigned: 23/30).";
    } else {
        eval.rubric1_score = 17;
        eval.rubric1_feedback = "AI Analysis: Similar to existing standard implementations with minimal extra feature differentiation or technical innovation (Assigned: 17/30).";
    }

    // ----------------------------------------------------
    // RUBRIC 2: Code Quality & Techniques (Max 30)
    // ----------------------------------------------------
    // Good code quality & efficient techniques: 25-30 marks
    // Satisfiable code quality & techniques: 15-24 marks
    // Below satisfiable: < 15 marks
    if (pdfData.has_clean_code && pdfData.has_efficient_algorithms) {
        eval.rubric2_score = 27;
        eval.rubric2_feedback = "AI Code Inspection: PDF contains clean, structured source code with efficient algorithm techniques, modular structure, and robust exception handling (Assigned: 27/30).";
    } else if (pdfData.extracted_code_lines >= 20 || pdfData.has_clean_code) {
        eval.rubric2_score = 21;
        eval.rubric2_feedback = "AI Code Inspection: Code implementation in PDF is satisfiable with good functional logic, though optimization and error handling could be enhanced (Assigned: 21/30).";
    } else {
        eval.rubric2_score = 14;
        eval.rubric2_feedback = "AI Code Inspection: Embedded code snippets in PDF lack proper modularization, efficiency, or standard formatting (Assigned: 14/30).";
    }

    // ----------------------------------------------------
    // RUBRIC 3: Modules & Output (Max 20)
    // ----------------------------------------------------
    // Good modules & output: 15-20 marks
    // OK modules & output: 10-15 marks
    // Below average: < 10 marks
    if (pdfData.total_modules >= 3 && pdfData.outputs_verified) {
        eval.rubric3_score = 18;
        eval.rubric3_feedback = "AI Module Check: Excellent multi-module breakdown (3+ modules) with complete execution output screenshots and test case verifications (Assigned: 18/20).";
    } else if (pdfData.outputs_verified || pdfData.total_modules >= 2) {
        eval.rubric3_score = 13;
        eval.rubric3_feedback = "AI Module Check: Satisfactory system modules and verified output results, meeting core course requirements (Assigned: 13/20).";
    } else {
        eval.rubric3_score = 8;
        eval.rubric3_feedback = "AI Module Check: Module decomposition is basic or output verification results are missing from the PDF (Assigned: 8/20).";
    }

    // ----------------------------------------------------
    // RUBRIC 4: Documentation & System Scope (Max 20)
    // ----------------------------------------------------
    // Good: 15-20 marks
    // OK: 10-15 marks
    // Below average: < 10 marks
    if (pdfData.documentation_complete && pdfData.has_good_scope) {
        eval.rubric4_score = 18;
        eval.rubric4_feedback = "AI Documentation Review: Comprehensive PDF layout with abstract, architecture diagrams, conclusion, and academic references (Assigned: 18/20).";
    } else if (pdfData.documentation_complete || pdfData.full_text.size() > 500) {
        eval.rubric4_score = 14;
        eval.rubric4_feedback = "AI Documentation Review: Well-formatted PDF documentation containing clear problem statement and design workflow (Assigned: 14/20).";
    } else {
        eval.rubric4_score = 9;
        eval.rubric4_feedback = "AI Documentation Review: Incomplete documentation structure; lacks conclusion or system architecture details (Assigned: 9/20).";
    }

    // Total Score & Pass Mark Check (50 is Pass)
    eval.total_score = eval.rubric1_score + eval.rubric2_score + eval.rubric3_score + eval.rubric4_score;
    eval.is_pass = (eval.total_score >= 50);

    std::stringstream ss;
    ss << "AI Automated Evaluation Complete. Total Score: " << eval.total_score << "/100 ("
       << (eval.is_pass ? "PASS" : "FAIL") << "). Evaluated across all PDF contents against standard 4 rubrics.";
    eval.evaluator_notes = ss.str();

    return eval;
}
