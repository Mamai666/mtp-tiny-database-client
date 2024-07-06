#ifndef SOCIDBCLIENT_H
#define SOCIDBCLIENT_H

#include "soci/soci.h"
#include "soci/postgresql/soci-postgresql.h"
#include "soci/sqlite3/soci-sqlite3.h"

#include "IDbClient.h"

enum SqlDBMS
{
    UNDEFINED = -1,
    PostgreSQL,
    MySQL,
    SQLite3
};

class SOCIDBClient : public IDBClient
{
public:
    SOCIDBClient(const SqlDBMS dbmsEnum);
    virtual ~SOCIDBClient(){}

    virtual bool connect(DBSettings& dbSets) {return false;};
    virtual bool disconnect();

    static std::string SqlDBMSFromEnum(SqlDBMS c)
    {
        switch (c) {
        case PostgreSQL: return "postgresql";
        case SQLite3: return "sqlite3";
        default: return "";
        }
    }

    static SqlDBMS SqlDBMSToEnum(std::string s)
    {
        if("postgresql") return PostgreSQL;
        else if("sqlite3") return SQLite3;
        else return UNDEFINED;
    }

    std::string sqlDBMS() const;

    virtual ErrStatus createTable(const std::string newTableName, std::vector<columnTableDb_t> columns);
    virtual ErrStatus renameTable(std::string oldTableName, std::string newTableName);
    virtual ErrStatus removeTable(std::string tableName);

    virtual ErrStatus updateColumn(std::string nameColumn, std::string value, std::string uniqKey, std::string uniqValue);
    virtual ErrStatus insertRow(std::vector<columnTableDb_t> columns, std::string uniqKey = "", std::string uniqValue = "");
    virtual ErrStatus addNewColumn(std::string nameColumn, std::string dataType);
    //virtual ErrStatus getRowFull(std::vector<JSON> &resultVarMap, std::string uniqKey = "", std::string uniqValue = "");
    virtual ErrStatus deleteRow(std::string nameField, std::string valueField);

    virtual ErrStatus listExistColumns(std::map<std::string, std::string> &existColumns);

    virtual ErrStatus getFilterRow(std::vector<JSON> &resultVarMap,
                                    std::map<std::string, std::string> filters,
                                   std::string needColumns = "*");

    virtual std::string nameTypeForTimestamp() { return "";}
    virtual std::string nameTypeForText() { return "";}
    virtual std::string nameTypeForJson() { return "";}
    virtual std::string nameTypeForBool() { return "";}
    virtual std::string nameTypeForInt() { return "";}
    virtual std::string nameTypeForDouble() { return "";}

    virtual std::string getTableName() const;
    virtual void        setTableName(std::string value, bool needQuotes = true);

    virtual ErrStatus   getTimeLine(std::vector<JSON> &resultVarMap,
                                  std::string &timeBegin, std::string &timeEnd, std::string &camSource)
    {
        return ErrStatus::NotExists;
    }

protected:
    bool _connect(std::string connectionString);
    bool checkConnectToDB();
    std::vector<std::string> getTableNames();

    std::string filterToStr(std::map<std::string, std::string> filters,
                            LIMIT_SET limSet = ON);

    JSON getAsJsonValue(const soci::row &row, size_t idx);

    soci::session m_sqlSession;

//private:
    std::string   m_sqlDBMS = "none";
    std::string   m_lastConnectionStr = "";
    std::string   m_tableName = "";
    const uint32_t c_MAXLIMIT = 300;
};

#endif // SOCIDBCLIENT_H
