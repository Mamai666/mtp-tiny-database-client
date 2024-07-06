#ifndef SQLITE3CLIENT_H
#define SQLITE3CLIENT_H

#include <inttypes.h>
#include <string>

#include "SOCIDBClient.h"


class SQLite3Client : public SOCIDBClient
{
public:
    SQLite3Client();
    ~SQLite3Client() {}

    virtual bool connect(DBSettings& dbSets);

    virtual std::string nameTypeForTimestamp();
    virtual std::string nameTypeForText();
    virtual std::string nameTypeForJson();
    virtual std::string nameTypeForBool();
    virtual std::string nameTypeForInt();
    virtual std::string nameTypeForDouble();
};

#endif // SQLITE3CLIENT_H
