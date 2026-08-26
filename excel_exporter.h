#ifndef EXCEL_EXPORTER_H
#define EXCEL_EXPORTER_H

#include "sql_database.h"
#include <string>

class ExcelExporter {
public:
    static std::string exportFacultyGradesToCSV(SQLDatabase& db, const std::string& faculty_username);
};

#endif
