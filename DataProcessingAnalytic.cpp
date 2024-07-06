#include "DataProcessingAnalytic.h"
#include "Timer.h"
#include "Utils/mstrings.h"
#include <map>

DataProcessingAnalytic::DataProcessingAnalytic(IDBClient* pdbCli) : IDataProcessing()
{
    p_dbCli = pdbCli;
    m_pBasicProc = new DataProcessingBasic(pdbCli);
}

DataProcessingAnalytic::~DataProcessingAnalytic()
{
    if(m_pBasicProc)
        delete m_pBasicProc;
}



JSON DataProcessingAnalytic::doWorkByRequest(JSON &jsonPack, const std::string &urlSender, const std::string &nameReqFunc)
{
    answerOnRequest_t answerPack;
    JSON outJson;
    outJson["status"] = false;

    std::string lastUsedTable = p_dbCli->getTableName();

    if(nameReqFunc == "analyticsRegister")
    {
        answerPack = registerAnalytics(jsonPack, urlSender);
    }
    else if(nameReqFunc == "metaData2Db")
    {
        answerPack = callWithVideoAnalytics(jsonPack);
    }
    else if(nameReqFunc == "getEmbeddingInfo")
    {
        answerPack = getEmbeddingInfo(jsonPack);
    }
    else {
        outJson["error"] = "Не найден требуемый запрос в if ("+nameReqFunc+") ";
        return outJson;
    }

    if(lastUsedTable != "")
        p_dbCli->setTableName(lastUsedTable);

    // ----------- Нужно постепенно отказаться от answerPack и писать напрямую в JSON --
    convertAnswerPackToJson(outJson, nameReqFunc, answerPack);
    // ---------------------------------------------------------

    return outJson;
}

answerOnRequest_t DataProcessingAnalytic::callWithVideoAnalytics(JSON &jsonPack)
{
    Timer tmr; tmr.start();
    answerOnRequest_t answerPack;
    answerPack.status = false;

    std::string eventDbTable = "";
    if(jsonPack.contains("AnalyticsType") || jsonPack.contains("analytics_type"))
    {
        eventDbTable = jsonPack.value("AnalyticsType", "");

        if(eventDbTable == "") {
            eventDbTable = jsonPack.value("analytics_type", "");
        }

        if(jsonPack.contains("AnalyticsSubType")) {
            LOG(WARNING) << "Внимание! Запрос старого формата с AnalyticsSubType!\n";
            eventDbTable += jsonPack.value("AnalyticsSubType", "empty_sub");
        }
        else if(jsonPack.contains("analytics_subType")) {
            eventDbTable += jsonPack.value("analytics_subType", "empty_sub");
        }
        else if(jsonPack.contains("analytics_sub_type")) {
            eventDbTable += jsonPack.value("analytics_sub_type", "empty_sub");
        }

        if(eventDbTable.empty())
        {
            answerPack.error = "Error! Analytics Json contains empty analytics_type field!";
            LOG(WARNING) << answerPack.error;
            return answerPack;
        }

        LOG(INFO) << "Analytics dbTable == " << eventDbTable;
    }
    else
    {
        if(jsonPack.value("request", "").find("metaData2Db") != std::string::npos)
        {
            answerPack.error = "Error! Analytics Json not contains analytics_type field!";
            LOG(WARNING) << answerPack.error;
            return answerPack;
        }
    }

    std::vector<JSON> pidRow;
    std::string source = jsonPack.value("source", "");
    if(!source.empty())
    {
        p_dbCli->setTableName("info_nodes");

        std::map<std::string, std::string> tmpFilters;
        tmpFilters["ip"] = source;
        p_dbCli->getFilterRow(pidRow, tmpFilters, "pid, name"); // Получаем первичный ключ устройства
        p_dbCli->setTableName(eventDbTable);

        if(pidRow.empty() && !jsonPack.contains("search_key")) {
            LOG(WARNING) << "Error: Pid source is empty!";
        }
        else if(!jsonPack.contains("search_key")) {
            jsonPack["pid_node"]  = pidRow.at(0).value("pid", -1) ;
            jsonPack["name_node"] = pidRow.at(0).value("name", "");
        }
    }
    else {
        LOG(WARNING) << "Внимание! Событие аналитики не содержит поле source!";
        p_dbCli->setTableName(eventDbTable);
    }

    LOG(DEBUG) << "Get pid_node elaps Mks : " << tmr.elapsedMicroseconds();

    if(jsonPack.contains("person_uuid")) {
        jsonPack.erase("info");
    }

    answerPack.status = m_pBasicProc->addNewRecord(jsonPack, true, true);
    LOG(DEBUG) << "End elaps Mks : " << tmr.elapsedMicroseconds();

    return answerPack;
}

IDBClient::ErrStatus DataProcessingAnalytic::updateEmbeddingInfo(JSON &jsonPack)
{
    LOG(ERROR) << "Ошибка! Вызов нереализованной функции! ";
    return IDBClient::UndefinedError;
}

answerOnRequest_t DataProcessingAnalytic::getEmbeddingInfo(JSON &jsonPack)
{
    LOG(ERROR) << "Ошибка! Вызов нереализованной функции! ";
    return answerOnRequest_t{};
}

answerOnRequest_t DataProcessingAnalytic::registerAnalytics(JSON &jsonPack, const std::string &urlSender)
{
    answerOnRequest_t answerPack;
    answerPack.status = false;

    IDBClient::ErrStatus errCode = IDBClient::ErrStatus::ErrSendQuery;
    Timer tmr; tmr.start();

    std::string targetTableName = "registeredanalytics";
    p_dbCli->setTableName(targetTableName);

    std::vector<JSON> tmpRowDataMaps;
    std::map<std::string, std::string> tmpFilters;
    auto moduleName = jsonPack.value("module_name", "");
    if(!moduleName.empty()) {
        tmpFilters["module_name"] = moduleName;
    }
    else {
        answerPack.error = "Пустое поле module_name";
        return answerPack;
    }

    auto proxyIp = jsonPack.value("proxy_ip", "");
    if(!proxyIp.empty()) {
        tmpFilters["proxy_ip"] = proxyIp;
    }
    else {
        LOG(WARNING) << "Empty proxy_ip! Replace on " << urlSender;
        proxyIp = urlSender;
    }

    std::string needColumns = "idx, comment";
    std::string saveComment = "-";
    errCode = p_dbCli->getFilterRow(tmpRowDataMaps, tmpFilters, needColumns);
    if(errCode == IDBClient::ErrStatus::NoError) // Здесь ошибка, значит удаляем запись и добавляем заново
    {
        for(const auto &itRow : tmpRowDataMaps)
        {
            saveComment = itRow.value("comment", "-");
            errCode = p_dbCli->deleteRow("idx", std::to_string(itRow.value("idx", 0)));
            if(errCode != IDBClient::ErrStatus::NoError) {
                answerPack.status = false;
                return answerPack;
            }
        }
    }

    JSON jsonOnWrite = jsonPack;
    jsonOnWrite["comment"]  = saveComment;
    jsonOnWrite["enable"]   = 1;
    jsonOnWrite["proxy_ip"] = proxyIp;
    //        jsonOnWrite["sensor_id"]          = jsonPack.value("sensor_id").toString();
    //        jsonOnWrite["source"]             = jsonPack.value("source").toString();
    //        jsonOnWrite["module_name"]        = jsonPack.value("module_name").toString();
    //        jsonOnWrite["subscription_types"] = jsonPack.value("subscription_types").toArray();
    //        jsonOnWrite["timestamp"]          = jsonPack.value("timestamp").toString();

    // m_uniqName = "module_name"; m_uniqValue = moduleName;
    answerPack.status = m_pBasicProc->addNewRecord(jsonOnWrite, true, true);

    LOG(DEBUG) << "End elaps Mks : " << tmr.elapsedMicroseconds();
    return answerPack;
}

std::vector<std::string> DataProcessingAnalytic::requestsList()
{
    return m_cSupportRequests;
}
