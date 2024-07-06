#ifndef POSTGRESQLCLIENT_H
#define POSTGRESQLCLIENT_H

#include <inttypes.h>
#include <string>

#include "SOCIDBClient.h"

// https://www.postgresql.org/docs/12/libpq-connect.html

class PostgreSQLClient : public SOCIDBClient
{
public:
    PostgreSQLClient();
    ~PostgreSQLClient();
    virtual bool connect(DBSettings& dbSets);

    virtual std::string nameTypeForTimestamp();
    virtual std::string nameTypeForText();
    virtual std::string nameTypeForJson();
    virtual std::string nameTypeForBool();
    virtual std::string nameTypeForInt();
    virtual std::string nameTypeForDouble();

    virtual ErrStatus   getTimeLine(std::vector<JSON> &resultVarMap,
                                    std::string &timeBegin, std::string &timeEnd, std::string &camSource);

private:
    ErrStatus createFunction(std::string text);

};

#endif // POSTGRESQLCLIENT_H
