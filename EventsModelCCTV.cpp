#include "EventsModelCCTV.h"
#include "loggerpp/include/LoggerPP.h"
#include <regex>

EventsModelCCTV::EventsModelCCTV(IDBClient *pdbCli) : EventBase(pdbCli)
{

}

std::string EventsModelCCTV::setFilterGetCCTVEvents(const JSON::array_t &cams, JSON &extFiltersObj,
                                                    FieldForFilterEvent& fEv,
                                                    std::map<std::string, std::string> &outFilters)
{
    std::string pidNodesStr = ""; std::string sensorIdsStr = "";
    std::string errText = "";

    if(!cams.empty())
    {
        if(!(cams.at(0).is_string() && cams.at(0) == "*"))
        {
            errText = fillNodesStrForEvent(cams, fEv, pidNodesStr, sensorIdsStr);
            if(!errText.empty()) {
                return errText;
            }

            outFilters.insert({"pid_node IN "+pidNodesStr, ""});
        }
    }

    std::string timeBegin   = "";
    std::string timeEnd     = "";

    if(extFiltersObj.is_object())
    {
        timeBegin = extFiltersObj.value("time-begin", "");
        timeEnd   = extFiltersObj.value("time-end", "");
    }

    if(!timeBegin.empty()) {
        fEv.timeBegin = timeBegin;
        extFiltersObj.erase("time-begin");
    }
    if(!timeEnd.empty()) {
        fEv.timeEnd = timeEnd;
        extFiltersObj.erase("time-end");
    }

    if(extFiltersObj.contains("exclude") && extFiltersObj.value("exclude", JSON{}).is_object())
    {
        auto excludeFilt = extFiltersObj.value("exclude", JSON{});
        for(auto itPrm : excludeFilt.items())
        {
            if(excludeFilt.value(itPrm.key(), JSON{}).is_string())
            {
                std::string prmVal = excludeFilt.value(itPrm.key(), "");
                if(!itPrm.key().empty())
                {
                    std::string filtStr = "\""+itPrm.key()+"\""+" NOT LIKE '%"+prmVal+"%'";
                    outFilters.insert({filtStr, ""});
                    LOG(DEBUG) << "Выбранное исключение: " << filtStr;
                }
            }
            else if(excludeFilt.value(itPrm.key(), JSON{}).is_array())
            {
                JSON::array_t paramArr = excludeFilt.value(itPrm.key(), JSON{});
                std::string inFindList = "";
                for(auto itPParam : paramArr)
                {
                    if(itPParam.is_string())
                    {
                        if(!inFindList.empty())
                        {
                            inFindList.append(",");
                        }
                        inFindList.append("'"+static_cast<std::string>(itPParam)+"'");
                    }
                }
                std::string filtStr = "\""+itPrm.key()+"\""+" NOT IN ("+inFindList+")";
                outFilters.insert({filtStr, ""});
                LOG(DEBUG) << "Выбранное исключение: " << filtStr;
            }
        }
    }

    for(const auto &itPrm : extFiltersObj.items())
    {
        if(extFiltersObj.value(itPrm.key(), JSON{}).is_string())
        {
            std::string prmVal = extFiltersObj.value(itPrm.key(), "");
            if(!itPrm.key().empty())
            {
                std::string filtStr = "\""+itPrm.key()+"\""+" LIKE '%"+prmVal+"%'";
                outFilters.insert({filtStr, ""});
                LOG(DEBUG) << "Выбранный фильтр: " << filtStr;
            }
        }
        else if(extFiltersObj.value(itPrm.key(), JSON{}).is_array())
        {
            JSON::array_t paramArr = extFiltersObj.value(itPrm.key(), JSON{});
            std::string inFindList = "";
            for(const auto &itPParam : paramArr)
            {
                if(itPParam.is_string())
                {
                    if(!inFindList.empty())
                    {
                        inFindList.append(",");
                    }
                    inFindList.append("'"+static_cast<std::string>(itPParam)+"'");
                }
            }
            std::string filtStr = "\""+itPrm.key()+"\""+" IN ("+inFindList+")";
            outFilters.insert({filtStr, ""});
            LOG(DEBUG) << "Выбранный фильтр: " << filtStr;
        }
    }

    if(!fEv.timeBegin.empty() && !fEv.timeEnd.empty()) {
        outFilters.insert({"(timestamp between '"+ fEv.timeBegin +"' and '"+fEv.timeEnd+"')", ""});
    }
    else if(fEv.timeBegin.empty() && !fEv.timeEnd.empty()) {
        outFilters.insert({"timestamp < '"+fEv.timeEnd+"'", ""});
    }
    else if(!fEv.timeBegin.empty() && fEv.timeEnd.empty()) {
        outFilters.insert({"timestamp > '"+fEv.timeBegin+"'", ""});
    }
    //    outFilters.insert("(timestamp between '"+ fEv.timeBegin +"' and '"+fEv.timeEnd+"')", "");
    if(fEv.statusView != "*") {
        outFilters.insert({"status", fEv.statusView});
    }

    outFilters.insert({"ORDER BY timestamp " + fEv.orderDirection, ""});

    fEv.limit = (fEv.limit > MAX_LIMIT) ? MAX_LIMIT : fEv.limit;
    if(fEv.limit > 0) {
        outFilters.insert({"LIMIT "+std::to_string(fEv.limit), ""});
    }
    else{
        outFilters.insert({"LIMIT "+std::to_string(MAX_LIMIT), ""});
    }

    return errText;
}

IDBClient::ErrStatus EventsModelCCTV::getDiffCCTVAnalyticEventsAsis(const InfoAboutAnalytic_t &infoAnalytic,
                                                                    std::string &needColumns,
                                                                    std::map<std::string, std::string> &filters,
                                                                    answerOnRequest_t &answerPack)
{
    std::vector<JSON> tmpDataMaps;
    p_dbCli->setTableName(infoAnalytic.nameTable); // установка таблицы аналитики, взятой из support_events

    LOG(DEBUG) << "Before getFilterRow..";
    IDBClient::ErrStatus errCode = p_dbCli->getFilterRow(tmpDataMaps, filters, needColumns);
    LOG(DEBUG) << "After getFilterRow..";

    if(tmpDataMaps.empty() && errCode == IDBClient::NotExists) {
        LOG(DEBUG) << "\n\t No update data events!\n";
        return IDBClient::ErrStatus::NotExists;
    }
    else if( errCode == IDBClient::NotCreateTable ){
        answerPack.error = "Not exist tablename == " + infoAnalytic.nameTable;
        answerPack.errorCode = IDBClient::NotCreateTable;
        LOG(WARNING) << answerPack.error << " !\n";
        return errCode;
    }
    else if( errCode != IDBClient::NoError ){
        answerPack.error = "Error get data events";
        LOG(WARNING) << answerPack.error << "!\n";
        return errCode;
    }

    for(auto &itOneEv : tmpDataMaps)
    {
        JSON oneData;
        for(auto &itKey : itOneEv.items())
        {
            if(itKey.value().is_null())
            {
                LOG(WARNING) << itKey.key() << " is NULL";
                continue;
            }
            oneData[itKey.key()] = itOneEv.value(itKey.key(), JSON{});
        }

        //oneData["frame-url"]        = itOneEv.value("path2screenshot").toString();
        oneData["type"]             = infoAnalytic.type;

        answerPack.dataMaps["data"].push_back(oneData);
    }
    //  answerPack.error = errText;
    return errCode;
}

IDBClient::ErrStatus EventsModelCCTV::getDiffCCTVAnalyticEvents(const InfoAboutAnalytic_t &infoAnalytic, std::string &needColumns,
                                                                std::map<std::string, std::string> &filters,
                                                                answerOnRequest_t &answerPack)
{
    std::vector<JSON> systemInfoDataMaps;
    p_dbCli->setTableName("info_system");
    IDBClient::ErrStatus errCode = p_dbCli->getFilterRow(systemInfoDataMaps, {});
    if( errCode == IDBClient::ErrStatus::NotExists ) {
        LOG(WARNING) << "Не удалось получить сведения о системе (пустая таблица info_system)!";
    }
    else if(errCode != IDBClient::ErrStatus::NoError) {
        answerPack.error = "Не удалось получить сведения о системе (не создана таблица info_system)!";
        LOG(WARNING) << answerPack.error;
        return errCode;
    }

    std::vector<JSON> tmpDataMaps;
    p_dbCli->setTableName(infoAnalytic.nameTable); // установка таблицы аналитики, взятой из support_events

    errCode = p_dbCli->getFilterRow(tmpDataMaps, filters, needColumns);
    if(tmpDataMaps.empty() && errCode != IDBClient::ErrSendQuery) {
        LOG(DEBUG) << "No update data events!\n";
        return IDBClient::ErrStatus::NotExists;
    }
    else if( errCode != IDBClient::ErrStatus::NoError ){
        //  answerPack.error = errText;
        LOG(WARNING) << "\n\t Error get data events!\n";
        return errCode;
    }

    std::string hostIp = ""; int hostPort = 0;
    for(auto &itSysInfo : systemInfoDataMaps)
    {
        if(itSysInfo.value("name", "") == "archive_props")
        {
            hostIp   = itSysInfo.value("ip_host", "");
            hostPort = itSysInfo.value("ip_port", 0);
        }
    }

    for(auto &itOneEv : tmpDataMaps)
    {
        JSON oneData;
        std::string time =  std::regex_replace(itOneEv.value("timestamp", ""), std::regex("T"), " ");

        //        oneData["frame-url"]        = itOneEv.value("path2screenshot").toString();
        //        oneData["frame-thumb"]      = itOneEv.value("screen_thumb").toString();
        //        oneData["event-frame-url"]  = itOneEv.value("path2crop").toString();
        //        oneData["event-frame-thumb"]= itOneEv.value("crop_thumb").toString();
        //        oneData["camera"]           = itOneEv.value("pid_node").toInt();
        //        oneData["sensor-id"]        = itOneEv.value("sensor_id").toString();

        JSON::array_t neighbourCams;
        auto cameraName = itOneEv.value("name_node", "");
        auto sensorName = itOneEv.value("sensor_id", "");

        //auto dateTime = QDateTime::fromString(time.toLower(), GTP_DATE_FORMAT);

        std::string mp4ArchiveUrl;
        const int16_t secBeforeEvt = 2, secAfterEvt = 2;
        if(!hostIp.empty() && hostPort > 0)
        {
//            mp4ArchiveUrl = std::string("http://%1:%2/%3?sensor=%4&start=%5&end=%6&format=mp4")
//                                .arg(hostIp, std::to_string(hostPort),
//                                     cameraName, sensorName,
//                                     dateTime.addSecs(-secBeforeEvt).toString(GTP_DATE_FORMAT).replace(" ", "_"),
//                                     dateTime.addSecs(secAfterEvt).toString(GTP_DATE_FORMAT).replace(" ", "_"));
        }

        JSON mainCam;
        mainCam["mp4-archive-url"] = mp4ArchiveUrl;
        mainCam["type-name"]       = "main_camera";
        mainCam["camera-name"]     = cameraName;
        mainCam["description"]     = "Главная камера";
        mainCam["frame-url"]       = itOneEv.value("path2screenshot", "");
        mainCam["frame-thumb"]     = itOneEv.value("screen_thumb", "");
        mainCam["crop-url"]        = itOneEv.value("path2crop", "");
        mainCam["crop-thumb"]      = itOneEv.value("crop_thumb", "");

        neighbourCams.push_back(mainCam);
        oneData["cameras"] = neighbourCams;

        oneData["event-comment"]   = infoAnalytic.description;
        oneData["type"]            = infoAnalytic.type;
        oneData["time"]            = time;
        oneData["info"]            = itOneEv.value("info", "");

        if(oneData.value("info", "").empty()) {
            if(itOneEv.contains("fullnumber")) { // TODO костыль для автономеров, переделать, когда будем править support_events*
                const std::string plate = itOneEv.value("fullnumber", "");
                const std::string country = itOneEv.value("country", "");
                oneData["info"] = plate + " (" + country + ")";
            }
        }

        if(itOneEv.contains("object_uuid")) {
            oneData["object-uuid"]  = itOneEv.value("object_uuid", "empty_object_uuid");
        }
        if(itOneEv.contains("person_uuid")) {
            const auto personUUID = itOneEv.value("person_uuid", "empty_person_uuid");
            oneData["person-uuid"]  = personUUID;
            oneData["info"] = personUUID;

            const std::string tableEmb = infoAnalytic.nameTable + "_emb";
            p_dbCli->setTableName(tableEmb);
            std::map<std::string, std::string> tmpFilters;
            tmpFilters.insert({"person_uuid", personUUID});
            tmpFilters.insert({"ORDER BY timestamp", ""});
            tmpFilters.insert({"LIMIT 1", ""});
            std::vector<JSON> tmpRowDataMaps;
            auto errCodeOk = p_dbCli->getFilterRow(tmpRowDataMaps, tmpFilters, "name");
            if(errCodeOk == IDBClient::ErrStatus::NoError && !tmpRowDataMaps.empty()) {
                oneData["info"] = tmpRowDataMaps.at(0).value("name", "");
                oneData["permGroup"] = tmpRowDataMaps.at(0).value("perm_group", "none");
            }
        }
        answerPack.dataMaps["data"].push_back(oneData);
    }
    //  answerPack.error = errText;
    return errCode;
}

