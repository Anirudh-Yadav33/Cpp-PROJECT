#include "pdf_parser.h"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iostream>

static std::string toLower(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower;
}

std::string PDFParser::cleanPDFText(const std::string& input) {
    std::string clean;
    clean.reserve(input.size());
    for (char c : input) {
        if (c >= 32 && c <= 126) clean += c;
        else if (c == '\n' || c == '\r' || c == '\t') clean += c;
        else clean += ' ';
    }
    return clean;
}

PDFAnalysisData PDFParser::parsePDFContent(const std::string& filename, const std::string& raw_content) {
    PDFAnalysisData data;
    data.filename = filename;
    data.full_text = cleanPDFText(raw_content);
    
    std::string lower = toLower(data.full_text);

    // Section 1: Title & Abstract Extraction
    std::size_t titlePos = lower.find("title:");
    if (titlePos != std::string::npos) {
        std::size_t endLine = data.full_text.find('\n', titlePos);
        data.extracted_title = data.full_text.substr(titlePos + 6, endLine - (titlePos + 6));
    } else {
        data.extracted_title = filename;
    }

    std::size_t absPos = lower.find("abstract");
    if (absPos != std::string::npos) {
        std::size_t nextSec = lower.find("1.", absPos);
        if (nextSec == std::string::npos) nextSec = absPos + 500;
        data.abstract = data.full_text.substr(absPos, nextSec - absPos);
    } else {
        data.abstract = data.full_text.substr(0, std::min((size_t)400, data.full_text.size()));
    }

    // Section 2: Literature Survey / Existing Project Comparison
    if (lower.find("existing") != std::string::npos || 
        lower.find("literature") != std::string::npos ||
        lower.find("prior work") != std::string::npos ||
        lower.find("novelty") != std::string::npos) {
        data.mentions_existing_system = true;
    }

    if (lower.find("extra feature") != std::string::npos ||
        lower.find("enhancement") != std::string::npos ||
        lower.find("additional features") != std::string::npos ||
        lower.find("difference from existing") != std::string::npos ||
        lower.find("advantages over existing") != std::string::npos ||
        lower.find("improved efficiency") != std::string::npos) {
        data.has_extra_features = true;
    }

    if (lower.find("high scope") != std::string::npos ||
        lower.find("future scope") != std::string::npos ||
        lower.find("scalable") != std::string::npos ||
        lower.find("impact") != std::string::npos ||
        lower.find("novel approach") != std::string::npos ||
        lower.find("unique project") != std::string::npos) {
        data.has_good_scope = true;
    }

    // Section 3: Source Code Quality & Techniques
    if (lower.find("code") != std::string::npos ||
        lower.find("algorithm") != std::string::npos ||
        lower.find("#include") != std::string::npos ||
        lower.find("class ") != std::string::npos ||
        lower.find("function") != std::string::npos ||
        lower.find("def ") != std::string::npos ||
        lower.find("import ") != std::string::npos) {
        
        // Count code lines estimate
        int lineCount = 0;
        std::stringstream ss(data.full_text);
        std::string line;
        while (std::getline(ss, line)) {
            if (line.find('{') != std::string::npos || line.find('}') != std::string::npos ||
                line.find(';') != std::string::npos || line.find("//") != std::string::npos ||
                line.find("return") != std::string::npos || line.find("for(") != std::string::npos) {
                lineCount++;
            }
        }
        data.extracted_code_lines = std::max(lineCount, 25);
    } else {
        data.extracted_code_lines = 15;
    }

    if (lower.find("clean code") != std::string::npos ||
        lower.find("oop") != std::string::npos ||
        lower.find("modular code") != std::string::npos ||
        lower.find("design patterns") != std::string::npos ||
        lower.find("exception handling") != std::string::npos ||
        lower.find("efficient") != std::string::npos ||
        lower.find("o(n)") != std::string::npos ||
        lower.find("optimization") != std::string::npos) {
        data.has_clean_code = true;
        data.has_efficient_algorithms = true;
    } else if (data.extracted_code_lines > 20) {
        data.has_clean_code = true;
    }

    // Section 4: System Modules & Execution Outputs
    int moduleCount = 0;
    if (lower.find("module 1") != std::string::npos || lower.find("module i") != std::string::npos) moduleCount++;
    if (lower.find("module 2") != std::string::npos || lower.find("module ii") != std::string::npos) moduleCount++;
    if (lower.find("module 3") != std::string::npos || lower.find("module iii") != std::string::npos) moduleCount++;
    if (lower.find("module 4") != std::string::npos || lower.find("module iv") != std::string::npos) moduleCount++;
    
    if (moduleCount == 0) {
        if (lower.find("modules") != std::string::npos || lower.find("architecture") != std::string::npos) {
            moduleCount = 3;
        } else {
            moduleCount = 2;
        }
    }
    data.total_modules = moduleCount;

    if (lower.find("output") != std::string::npos ||
        lower.find("results") != std::string::npos ||
        lower.find("screenshot") != std::string::npos ||
        lower.find("accuracy") != std::string::npos ||
        lower.find("execution") != std::string::npos ||
        lower.find("test case") != std::string::npos) {
        data.outputs_verified = true;
    }

    if (lower.find("conclusion") != std::string::npos && lower.find("reference") != std::string::npos) {
        data.documentation_complete = true;
    }

    return data;
}
