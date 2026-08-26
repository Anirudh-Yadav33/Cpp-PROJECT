#include "excel_exporter.h"

std::string ExcelExporter::exportFacultyGradesToCSV(SQLDatabase& db, const std::string& faculty_username) {
    return db.generateCSVExport(faculty_username);
}
