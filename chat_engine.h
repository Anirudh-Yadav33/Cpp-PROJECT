#ifndef CHAT_ENGINE_H
#define CHAT_ENGINE_H

#include "sql_database.h"
#include <string>

class ChatEngine {
public:
    static ChatMessage processFacultyMessage(SQLDatabase& db, int submission_id, const std::string& faculty_message);
};

#endif
