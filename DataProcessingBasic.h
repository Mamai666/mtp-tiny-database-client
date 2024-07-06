#ifndef DATAPROCESSINGBASE_H
#define DATAPROCESSINGBASE_H

#define GTP_DATE_FORMAT "yyyy-MM-dd hh:mm:ss.zzz"

#include "IDataProcessing.h"
#include "IDbClient.h"
#include "Utils/mstrings.h"
#include <list>

#include "loggerpp/include/LoggerPP.h"

struct answerOnRequest_t
{
    // answerOnRequest_t() {
    //     dataMaps.resize(1);
    // }

    std::string request;
    bool status;
    std::string error = "";
    int errorCode = IDBClient::NoHaveErrorCode;
    //std::vector<std::vector<JSON>> dataMaps;
    //std::vector<std::string> dataList;

    std::map<std::string, std::vector<JSON>> dataMaps;
    std::string timeLastUPD = "";
};

static void convertAnswerPackToJson(JSON &outJson, const std::string &nameReqFunc,
                             answerOnRequest_t &answerPack)
{
    //LOG(WARNING) << "ВНИМАНИЕ! Метод convertAnswerPackToJson отключен!!!";
     LOG(DEBUG) << "Begin convertAnswerPackToJson..";

    for(auto &item : answerPack.dataMaps)
    {
        outJson[item.first] = item.second;
    }

    // std::vector<JSON> dataArr;
    // if(!answerPack.dataMaps.empty())
    // {
    //     dataArr.resize(answerPack.dataMaps.size());
    //     for(int i = 0; i < answerPack.dataMaps.size(); ++i)
    //     {
    //         for(const auto &itV : answerPack.dataMaps[i])
    //         {
    //             JSON dataObj = itV; //JSON::from_msgpack( QJsonObject::fromVariantMap(itV);
    //             dataArr[i].push_back(dataObj);
    //         }
    //     }
    // }
    // if(!answerPack.timeLastUPD.empty()) {
    //     outJson["time-last"] = answerPack.timeLastUPD;
    // }
    // if(Strings::startsWith(nameReqFunc, "archive-timeline")) {
    //     outJson["available-ranges"]   = dataArr.at(0);
    //     outJson["ranges-with-events"] = dataArr.at(1);
    // }
    // else if(Strings::startsWith(nameReqFunc, "getEmbedding")) {
    //     outJson["embeddings"] = dataArr.at(0);
    // }
    // else {
    //     outJson["data"] = dataArr.at(0);
    // }

    // JSON listArray;
    // if(!answerPack.dataList.empty())
    // {
    //     listArray = answerPack.dataList;
    //     outJson["items"] = listArray;
    // }

    if(!answerPack.error.empty()) {
        LOG(ERROR) << "answerPack.error: " << answerPack.error;
        outJson["error"] = answerPack.error;
        outJson["error-code"] = answerPack.errorCode;
        outJson["status"] = false;
    }
    else outJson["status"] = answerPack.status;

    LOG(DEBUG) << "END convertAnswerPackToJson..";
}

//static bool timeLastEventMoreThan(const QVariantMap &qm1, const QVariantMap &qm2)
//{
//    QDateTime dt1; dt1 = QDateTime::fromString(qm1.value("time").toString().toLower(), GTP_DATE_FORMAT);
//    QDateTime dt2; dt2 = QDateTime::fromString(qm2.value("time").toString().toLower(), GTP_DATE_FORMAT);
//    // qDebug() << qm1.value("time").toString().toLower() << GTP_DATE_FORMAT << dt1;
//    return dt1 > dt2;
//}

//static bool timeLastEventLessThan(const QVariantMap &qm1, const QVariantMap &qm2)
//{
//    QDateTime dt1; QDateTime dt2;
//    if(qm1.contains("time")) {
//        dt1 = QDateTime::fromString(qm1.value("time").toString().toLower(), GTP_DATE_FORMAT);
//        dt2 = QDateTime::fromString(qm2.value("time").toString().toLower(), GTP_DATE_FORMAT);
//    }
//    // qDebug() << qm1.value("time").toString().toLower() << GTP_DATE_FORMAT << dt1;
//    return dt1 < dt2;
//}

class DataProcessingBasic : public IDataProcessing
{
public:
    DataProcessingBasic(IDBClient* pdbCli);
    virtual ~DataProcessingBasic(){}

    virtual JSON doWorkByRequest(JSON &jsonPack, const std::string &urlSender,
                                 const std::string& nameReqFunc);

    IDBClient::ErrStatus updateProc(JSON& jsonPack, std::string searchKeyName, JSON searchKeyValue);
    bool addNewRecord(JSON jsonPack, bool flagWriteAll = false, bool addIndexColumn = true);
    IDBClient::ErrStatus getAll(JSON& jsonPack,
                                std::string searchKeyName, JSON searchKeyValue,
                                answerOnRequest_t &answerPack);

    IDBClient::ErrStatus deleteProc(JSON& jsonPack, std::string searchKeyName, JSON searchKeyValue);

protected:
    virtual std::vector<std::string> requestsList();

private:
    IDBClient* p_db = nullptr;

    std::vector<std::string> m_blackColumnList = {"mtp", "gtp", "request",
        "unique-name", "unique-value",
        //"AnalyticsType", "AnalyticsSubType",
        //"analytics_type", "analytics_subType",
        //"analytics_sub_type",
        "zbase64_img"
    };

    const std::map<std::string, std::string> m_cSystemPasswd{
        {"initial_user", "terlus"},
        {"initial_user@", "1qaz@WSX"},
        {"userRO@", "a1234567*"},
        {"userRW@", "a1234567*"},
        {"admin@", "terlus"}
    };

    const std::vector<std::string> m_cSupportRequests =
    {
        "update",
        "addNewRecord",
        "getAll",
        "delete",
        "deleteLayout"
    };

    void add(JSON::iterator& it, JSON::array_t &format, std::vector<columnTableDb_t> &columnsTable);

    columnTableDb_t defineTypeAndValue(std::string key, JSON value, JSON::array_t &format);
    columnTableDb_t autoDefineType(std::string &key, JSON &value);
    columnTableDb_t formatDefineType(std::string &key, JSON &value, JSON::array_t &format);

    void extractValueFromObject(JSON jsObj, JSON::array_t &format, std::vector<columnTableDb_t> &columnsTable,
                                std::string &uniqName, std::string &uniqValue);

    bool putValueInTable(std::vector<columnTableDb_t> &columnsTable,
                         const std::string &uniqName, const std::string &uniqValue);

};

#endif // DATAPROCESSINGBASE_H
