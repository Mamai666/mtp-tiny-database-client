#include "SQLite3Client.h"

SQLite3Client::SQLite3Client() : SOCIDBClient(SqlDBMS::SQLite3)
{

}

bool SQLite3Client::connect(DBSettings &dbSets)
{
    std::string conStr = "db="+dbSets.nameDB+
                         " timeout="+std::to_string(dbSets.conTimeout)+
                         " shared_cache=true";

    bool retOK = _connect(conStr);

    return retOK;
}

std::string SQLite3Client::nameTypeForTimestamp()
{
    return "TEXT";
}

std::string SQLite3Client::nameTypeForText()
{
    return "TEXT";
}

std::string SQLite3Client::nameTypeForJson()
{
    return "TEXT";
}

std::string SQLite3Client::nameTypeForBool()
{
    return "INTEGER";
}

std::string SQLite3Client::nameTypeForInt()
{
    return "INTEGER";
}

std::string SQLite3Client::nameTypeForDouble()
{
    return "REAL";
}
