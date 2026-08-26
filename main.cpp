#include <iostream>
#include <csignal>
#include <windows.h>

#include "sql_database.h"
#include "http_server.h"
#include "ai_evaluator.h"

static HTTPServer* globalServer = nullptr;

BOOL WINAPI consoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT) {
        std::cout << "\n[System] Shutdown signal received. Stopping C++ server...\n";
        if (globalServer) {
            globalServer->stop();
        }
        ExitProcess(0);
    }
    return TRUE;
}

void seedDemoSubmissions(SQLDatabase& db) {
    ProjectSubmission s1;
    if (!db.getSubmissionById(1, s1)) {
        std::cout << "[System Seeder] Seeding initial student project submissions into SQL database...\n";
        
        ProjectSubmission sub1;
        sub1.course_id = 1; // Advanced AI & Machine Learning
        sub1.student_username = "alex_student";
        sub1.student_name = "Alex Johnson";
        sub1.title = "AI-Driven Healthcare Disease Risk Predictor";
        sub1.description = "A deep learning neural network project using convolutional neural networks for early medical image diagnostic evaluation.";
        sub1.pdf_filename = "Alex_Johnson_AI_Project_Report.pdf";
        sub1.special_code = "84920";
        sub1.pdf_content = 
            "TITLE: AI-Driven Healthcare Disease Risk Predictor\n"
            "AUTHOR: Alex Johnson (Student ID: STU-2026-09)\n"
            "COURSE: Advanced AI & Machine Learning\n\n"
            "ABSTRACT:\n"
            "This project presents a novel artificial intelligence architecture for real-time diagnostic evaluation. "
            "Compared to traditional existing diagnostic tools, our project introduces high scope transfer learning with unique features.\n\n"
            "1. LITERATURE SURVEY & EXISTING SYSTEM DIFFERENCES:\n"
            "Existing healthcare models rely on static thresholding. Our project introduces dynamic extra features, "
            "including real-time multi-modal tensor integration and automated confidence calibration. "
            "This project is not existing in current literature in this combined configuration and provides immense future scope.\n\n"
            "2. SYSTEM MODULE ARCHITECTURE:\n"
            "Module 1: Data Preprocessing & Augmentation Engine\n"
            "Module 2: ResNet-50 Feature Extraction & Convolutional Classification\n"
            "Module 3: Online Inference API & Interactive Web Visualizer\n\n"
            "3. SOURCE CODE SNIPPETS & ALGORITHMS:\n"
            "```cpp\n"
            "#include <iostream>\n"
            "#include <vector>\n"
            "class NeuralPredictor {\n"
            "private:\n"
            "    std::vector<float> weights;\n"
            "public:\n"
            "    NeuralPredictor(int inputs) : weights(inputs, 0.05f) {}\n"
            "    float forward(const std::vector<float>& features) {\n"
            "        float score = 0.0f;\n"
            "        for(size_t i = 0; i < features.size(); ++i) {\n"
            "            score += features[i] * weights[i];\n"
            "        }\n"
            "        return 1.0f / (1.0f + exp(-score)); // Sigmoid activation\n"
            "    }\n"
            "};\n"
            "```\n\n"
            "4. OUTPUTS & VERIFICATION RESULTS:\n"
            "Execution test cases verified on 10,000 dataset samples. Model accuracy: 96.4%. "
            "Outputs verified with confusion matrix screenshots and execution log tables.\n\n"
            "5. CONCLUSION & REFERENCES:\n"
            "The system offers clean code quality, complete documentation, modular design, and robust predictive performance.";

        db.createSubmission(sub1);
        
        // Auto-run AI evaluation for sub1
        EvaluationRecord aiEval1 = AIEvaluator::evaluateSubmission(sub1);
        db.saveEvaluation(aiEval1);

        ProjectSubmission sub2;
        sub2.course_id = 1; // Advanced AI & Machine Learning
        sub2.student_username = "sarah_connor";
        sub2.student_name = "Sarah Connor";
        sub2.title = "Autonomous Robotic Path Planning using Q-Learning";
        sub2.description = "Reinforcement learning navigation algorithm for autonomous mobile robots in complex dynamic environments.";
        sub2.pdf_filename = "Sarah_Connor_Robot_PathPlanning.pdf";
        sub2.special_code = "84920";
        sub2.pdf_content = 
            "TITLE: Autonomous Robotic Path Planning using Q-Learning\n"
            "AUTHOR: Sarah Connor\n"
            "ABSTRACT:\n"
            "An autonomous path planner using deep Q-networks. Evaluates spatial grid movement with obstacle avoidance.\n\n"
            "1. EXISTING SYSTEM COMPARISON:\n"
            "Enhances standard Dijkstra algorithm with dynamic Q-table updates. Includes extra features for real-time obstacle map updates.\n\n"
            "2. MODULES & OUTPUT:\n"
            "Module 1: Environment Grid Simulator\n"
            "Module 2: Reinforcement Learning Q-Agent\n"
            "Module 3: Path Visualization Module\n\n"
            "3. SOURCE CODE:\n"
            "```cpp\n"
            "double updateQ(double reward, double maxNextQ, double currentQ) {\n"
            "    return currentQ + 0.1 * (reward + 0.9 * maxNextQ - currentQ);\n"
            "}\n"
            "```\n\n"
            "4. RESULTS & CONCLUSION:\n"
            "Path converges in 150 iterations. Documentation complete.";

        db.createSubmission(sub2);
        EvaluationRecord aiEval2 = AIEvaluator::evaluateSubmission(sub2);
        db.saveEvaluation(aiEval2);

        std::cout << "[System Seeder] Initial project submissions & AI evaluations seeded successfully!\n";
    }
}

int main() {
    std::cout << "========================================================\n";
    std::cout << "  PROJECT EVALUATION SYSTEM USING AI - C++ & SQL\n";
    std::cout << "========================================================\n";

    SetConsoleCtrlHandler(consoleHandler, TRUE);

    SQLDatabase db("project_eval.db");
    if (!db.init()) {
        std::cerr << "[Fatal] Could not initialize SQL Database!\n";
        return 1;
    }
    std::cout << "[SQL Engine] SQLite Database ('project_eval.db') initialized & active.\n";

    seedDemoSubmissions(db);

    int port = 8080;
    HTTPServer server(port, db);
    globalServer = &server;

    if (!server.start()) {
        std::cerr << "[Fatal] Server failed to start on port " << port << std::endl;
        return 1;
    }

    std::cout << "\n>>> Web Application live at: http://localhost:" << port << " <<<\n";
    std::cout << "Press Ctrl+C to terminate server.\n\n";

    while (true) {
        Sleep(1000);
    }

    return 0;
}
