#ifndef IDBCLIENT_H
#define IDBCLIENT_H

#include <inttypes.h>
#include <json/TBaseJsonWork.h>

#define MAX_LIMIT 100000

struct DBSettings
{
    std::string host = "";
    std::string nameDB = "";
    std::string userName = "";
    std::string password = "";
    uint16_t    port = 0;
    uint16_t    conTimeout = 5;
    std::map<std::string, std::string> other;

    bool isValid() const
    {
        return !host.empty() &&
               !nameDB.empty() &&
               !userName.empty() &&
               !password.empty() &&
               !(port > 0) &&
               !(conTimeout > 0);
    }

    //    static QString encodeStr(const QString& str)
    //    {
    //        QByteArray arr(str.toUtf8());
    //        for(int i =0; i<arr.size(); i++)
    //            arr[i] = arr[i] ^ keyPass;

    //        return QString::fromLatin1(arr.toBase64());
    //    }

    //    static QString decodeStr(const QString &str)
    //    {
    //        QByteArray arr = QByteArray::fromBase64(str.toLatin1());
    //        for(int i =0; i<arr.size(); i++)
    //            arr[i] =arr[i] ^ keyPass;

    //        return QString::fromUtf8(arr);
    //    }

    void deserializeFromJson(const JSON& inJs) {
        host       = inJs.value("Host", "");
        nameDB     = inJs.value("Name", "");
        port       = inJs.value("Port", 5432);
        password   = inJs.value("Password", ""); //decodeStr(inJs.value("Password").toString(""));
        userName   = inJs.value("UserName", "");
        conTimeout = inJs.value("ConnectTimeout", 5);
    }

    void serializeToJson(JSON& outJs) {
        outJs["Host"]           = host;
        outJs["Name"]           = nameDB;
        outJs["Port"]           = port;
        outJs["Password"]       = password;//encodeStr(password);
        outJs["UserName"]       = userName;
        outJs["ConnectTimeout"] = conTimeout;
    }

    friend std::ostream& operator<< (std::ostream &out, const DBSettings &db)
    {
        out << "\nSettings database:\n"
            << "host == " << db.host << " ;\n"
            << "name == "  << db.nameDB << " ;\n"
            << "username == " << db.userName << " ;\n"
            << "password == " << db.password << " ;\n";

        return out;
    }

    void operator=(const DBSettings& r_val)
    {
        this->host      = r_val.host;
        this->nameDB    = r_val.nameDB;
        this->port = r_val.port;
        this->password = r_val.password;
        this->userName = r_val.userName;
        this->conTimeout = r_val.conTimeout;
    }

    friend bool operator== (DBSettings const& lhs, DBSettings const& rhs)
    {
        bool equal = false;
        if(lhs.host == rhs.host &&
            lhs.nameDB      == rhs.nameDB &&
            lhs.userName    == rhs.userName &&
            lhs.password    == rhs.password &&
            lhs.port        == rhs.port &&
            lhs.conTimeout  == rhs.conTimeout)
        {
            equal = true;
        }

        return equal;
    }

    friend bool operator!= (DBSettings const& lhs, DBSettings const& rhs) {
        return !(lhs == rhs);
    }

private:
    static const int keyPass = 0734455;
};

struct columnTableDb_t
{
    columnTableDb_t() {
        this->position = 1000;
    }

    int position ;
    std::string name ;
    std::string dataType ;
    std::string value ;
};

class IDBClient
{
public:
    enum LIMIT_SET{
        OFF = 0,
        ON = 1
    };

    enum ErrStatus{
        ErrConnectDB = -10,
        ErrSendQuery,
        NotCreateTable,
        UndefinedError = -1,

        AlreadyExists,
        NotExists,
        NoError,
        NoHaveErrorCode = 1995
    };

    IDBClient(){}
    virtual ~IDBClient(){}

    virtual bool connect(DBSettings& dbSets) = 0;
    virtual bool disconnect() = 0;

    virtual std::string nameTypeForTimestamp() = 0;
    virtual std::string nameTypeForText() = 0;
    virtual std::string nameTypeForJson() = 0;
    virtual std::string nameTypeForBool() = 0;
    virtual std::string nameTypeForInt() = 0;
    virtual std::string nameTypeForDouble() = 0;

    virtual std::string getTableName() const = 0;
    virtual void        setTableName(std::string value, bool needQuotes = true) = 0;

    //****** ORDINARY DATABASE (SYSTEM) *********
    virtual ErrStatus createTable(const std::string newTableName, std::vector<columnTableDb_t> columns) = 0;
    virtual ErrStatus updateColumn(std::string nameColumn, std::string value, std::string uniqKey, std::string uniqValue) = 0;
    virtual ErrStatus insertRow(std::vector<columnTableDb_t> columns, std::string uniqKey = "", std::string uniqValue = "") = 0;
    virtual ErrStatus addNewColumn(std::string nameColumn, std::string dataType) = 0;
    //ErrStatus getRowFull(std::vector<QVariantMap> &resultVarMap, std::string uniqKey = "", std::string uniqValue = "");
    virtual ErrStatus deleteRow(std::string nameField, std::string valueField) = 0;

    virtual ErrStatus renameTable(std::string oldTableName, std::string newTableName) = 0;
    virtual ErrStatus removeTable(std::string tableName) = 0;

    // --------------------------------------------------------------------------------------------------

    virtual ErrStatus getTimeLine(std::vector<JSON> &resultVarMap,
                                  std::string &timeBegin, std::string &timeEnd, std::string &camSource) = 0;

    virtual ErrStatus listExistColumns(std::map<std::string, std::string> &existColumns) = 0;
    virtual ErrStatus getFilterRow(std::vector<JSON> &resultVarMap,
                                    std::map<std::string, std::string> filters,
                                    std::string needColumns = "*") = 0;

protected:
    DBSettings m_lastDbSets;
};

#endif // IDBCLIENT_H
