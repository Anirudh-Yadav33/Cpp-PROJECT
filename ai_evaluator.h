#ifndef AI_EVALUATOR_H
#define AI_EVALUATOR_H

#include "sql_database.h"
#include "pdf_parser.h"

class AIEvaluator {
public:
    static EvaluationRecord evaluateSubmission(const ProjectSubmission& sub);
};

#endif
