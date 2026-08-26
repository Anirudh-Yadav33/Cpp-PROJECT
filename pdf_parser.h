#ifndef PDF_PARSER_H
#define PDF_PARSER_H

#include <string>
#include <vector>

struct PDFAnalysisData {
    std::string filename;
    std::string full_text;
    std::string extracted_title;
    std::string abstract;
    std::string existing_project_section;
    std::string code_snippets_section;
    std::string modules_section;
    std::string output_results_section;
    
    // Extracted Metrics
    bool mentions_existing_system = false;
    bool has_extra_features = false;
    bool has_good_scope = false;
    
    int extracted_code_lines = 0;
    bool has_clean_code = false;
    bool has_efficient_algorithms = false;
    
    int total_modules = 0;
    bool outputs_verified = false;
    bool documentation_complete = false;
};

class PDFParser {
public:
    static PDFAnalysisData parsePDFContent(const std::string& filename, const std::string& raw_content);
    static std::string cleanPDFText(const std::string& input);
};

#endif
