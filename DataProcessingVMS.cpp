#include "DataProcessingVMS.h"

DataProcessingVMS::DataProcessingVMS(IDBClient *pdbCli) : IDataProcessing()
{
    p_dbCli = pdbCli;
    m_pBasicProc = new DataProcessingBasic(pdbCli);
    m_pEventProc  = new DataProcessingEvent(pdbCli);
}

DataProcessingVMS::~DataProcessingVMS()
{
    if(m_pBasicProc)
        delete m_pBasicProc;

    if(m_pEventProc)
        delete m_pEventProc;
}

std::vector<std::string> DataProcessingVMS::requestsList()
{
    return m_cSupportRequests;
}

JSON DataProcessingVMS::doWorkByRequest(JSON &jsonPack, const std::string &urlSender, const std::string &nameReqFunc)
{
    answerOnRequest_t answerPack;
    JSON outJson;
    outJson["status"] = false;

    std::string lastUsedTable = p_dbCli->getTableName();

    if(nameReqFunc == "read-archive")
    {
        answerPack = readArchive(jsonPack);
    }
    else if(nameReqFunc == "archive-timeline-cctv")
    {
        answerPack = archiveTimeline(jsonPack);
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

answerOnRequest_t DataProcessingVMS::readArchive(JSON &jsonPack)
{
    answerOnRequest_t answerPack;
    answerPack.status = false;

    std::string tableName = "archivewriter";
    if(jsonPack.contains("table")){
        tableName = jsonPack.value("table", "NONE");
    }
    p_dbCli->setTableName(tableName);

    std::string sensorToken     = jsonPack.value("sensor-id", "");
    std::string cameraName      = jsonPack.value("camera-name", "");
    std::string sourceName      = jsonPack.value("source", "");
    //int     streamNumber    = jsonPack.value("stream-number").toInt(-1);
    std::string dateTimeBegin   = jsonPack.value("datetime-begin", "");
    std::string dateTimeEnd     = jsonPack.value("datetime-end", "");

    std::map<std::string, std::string> filters;
    std::string timeBeginFilt   = "(\"timestamp-begin\" between '"+ dateTimeBegin +"' and '"+dateTimeEnd+"')";
    std::string timeEndFilt     = "(\"timestamp-end\" between '"+ dateTimeBegin +"' and '"+dateTimeEnd+"')";

    if(!cameraName.empty()) {
        filters["\"camera-name\""] = cameraName;
    }

    if(!sourceName.empty()) {
        filters["\"source\""] = sourceName;
    }
//    if(streamNumber > 0) {
//        filters.insert("\"stream-number\"", QString::number(streamNumber));
//    }
    if(!sensorToken.empty()) {
        filters["\"sensor-id\""] = sensorToken;
    }
    filters[timeBeginFilt]= "";
    filters[timeEndFilt] = "";

    filters["ORDER BY idx "] = jsonPack.value("order", "asc");

    std::vector<JSON> tmpDataMaps;
    tmpDataMaps.clear();
    auto retErr = p_dbCli->getFilterRow(tmpDataMaps, filters, "path2video");
    if(tmpDataMaps.empty()) {
        LOG(WARNING) << "Not have record by this filter in archive!\n";
        answerPack.dataMaps["items"] = JSON::array_t();
        answerPack.status = false;
    }

    for(auto &itOneRec : tmpDataMaps)
    {
        if(!itOneRec.value("path2video", "").empty())
        {
            answerPack.dataMaps["items"].push_back(itOneRec["path2video"]);
            answerPack.status = true;
        }
    }

    std::map<std::string, JSON> sourceMap;
    sourceMap.insert({"camera-name", cameraName});
    //sourceMap.insert("stream-number", streamNumber);
    sourceMap.insert({"sensor-id", sensorToken});
    sourceMap.insert({"datetime-begin", dateTimeBegin});
    sourceMap.insert({"datetime-end", dateTimeEnd});

    answerPack.dataMaps["data"].push_back(sourceMap);

    return answerPack;
}

answerOnRequest_t DataProcessingVMS::archiveTimeline(JSON &jsonPack)
{
    answerOnRequest_t answerPack;
    answerPack.status = false;

    std::string timeBegin       = jsonPack.value("time-begin", "1970-01-01 12:00:00");
    std::string timeEnd         = jsonPack.value("time-end", "1970-01-01 13:00:00");
    std::string camSource       = jsonPack.value("source", "none");

    std::vector<JSON> tmpDataMaps;
    IDBClient::ErrStatus retErr = p_dbCli->getTimeLine(tmpDataMaps, timeBegin, timeEnd, camSource);
    if(!tmpDataMaps.empty() && retErr == IDBClient::ErrStatus::NoError)
    {
        for(auto &itTimeSub : tmpDataMaps) {
            JSON oneDataMap;
            oneDataMap["start"]  = itTimeSub.value("begin-sub", "");
            oneDataMap["end"]    = itTimeSub.value("end-sub", "");
            answerPack.dataMaps["available-ranges"].push_back(oneDataMap);
        }
        answerPack.status = true;

    //-------------- GET HAPPEN-EVENT--------------------------------
        // Реализовать по новому использование запроса событий за этот период
        answerPack.dataMaps["ranges-with-events"] = JSON::array_t{};

        answerOnRequest_t eventAnswPack;
        JSON eventJsonReq;
        eventJsonReq["request"] = "archive-event-cctv@asis";
        //eventJsonReq["time-begin"] = "1970-01-01 12:00:00";
        //eventJsonReq["time-end"]   = "2025-01-01 12:00:00";
        eventJsonReq["limit"] = jsonPack.value("limit", 3000);

        JSON filtersObj;
        filtersObj["source"] = camSource;

        eventJsonReq["filters"] = filtersObj;

        eventJsonReq["need-columns"] = JSON::array_t{"timestamp", "analytics_type"};
        eventJsonReq["types"] = jsonPack.value("types",  JSON::array_t{"*"});

        try {
            retErr = m_pEventProc->realTimeArchiveEventCCTV(eventJsonReq, eventJsonReq["request"], eventAnswPack);

            if(retErr == IDBClient::NoError)
            {
                for(const auto &itEv : eventAnswPack.dataMaps["data"])
                {
                    JSON oneHappen;
                    oneHappen["happen-what"] = itEv.value("analytics_type", "none");
                    oneHappen["happen-at"]   = itEv.value("timestamp", "none");
                    answerPack.dataMaps["ranges-with-events"].push_back(oneHappen);
                }
            }
            else {
                LOG(WARNING) << "event retErr: " << retErr;
            }

        }
        catch(dataproc_error &ed) {
            answerPack.error = "dataproc_error exception: "+std::string(ed.what());
        }
        catch(std::exception &es) {
            answerPack.error = "11Std exception: "+std::string(es.what());
        }

        if(retErr != IDBClient::ErrSendQuery && retErr != IDBClient::ErrConnectDB && retErr != IDBClient::UndefinedError) {
            answerPack.status = true;
        }
        else if(eventAnswPack.error.empty()) {
            answerPack.error = "Any ErrStatus";//metaEnum.valueToKey(retErr);
        }

    //-------------------End GET HAPPEN-Event---------------------
    }
    else if(retErr == IDBClient::ErrSendQuery){
        answerPack.error = "getTimeLine return empty and or Error!";
        answerPack.status = false;
    }

    return answerPack;
}
