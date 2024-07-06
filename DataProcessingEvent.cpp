#include "DataProcessingEvent.h"
#include "Timer.h"

DataProcessingEvent::DataProcessingEvent(IDBClient* pdbCli) : IDataProcessing()
{
    p_dbCli = pdbCli;
    m_pBasicProc = new DataProcessingBasic(pdbCli);
    m_upEventModelCCTV = std::make_unique<EventsModelCCTV>(p_dbCli);
}

DataProcessingEvent::~DataProcessingEvent()
{
    if(m_pBasicProc)
        delete m_pBasicProc;
}

JSON DataProcessingEvent::doWorkByRequest(JSON &jsonPack, const std::string &urlSender, const std::string &nameReqFunc)
{
    answerOnRequest_t answerPack;
    JSON outJson;
    outJson["status"] = false;

    IDBClient::ErrStatus retErr = IDBClient::UndefinedError;
    std::string lastUsedTable = p_dbCli->getTableName();

    try {

    if(nameReqFunc == "realtime-event-cctv" || nameReqFunc == "archive-event-cctv")
    {
        retErr = realTimeArchiveEventCCTV(jsonPack, nameReqFunc, answerPack);
    }
    if(nameReqFunc == "realtime-event-cctv@asis" || nameReqFunc == "archive-event-cctv@asis")
    {
        retErr = realTimeArchiveEventCCTV(jsonPack, nameReqFunc, answerPack);
    }
    else {
        outJson["error"] = "Не найден требуемый запрос в if ("+nameReqFunc+") ";
        return outJson;
    }

    }
    catch(dataproc_error &ed) {
        answerPack.error = "dataproc_error exception: "+std::string(ed.what());
    }
    catch(std::exception &es) {
        answerPack.error = "Std exception: "+std::string(es.what());
    }

    if(retErr != IDBClient::ErrSendQuery && retErr != IDBClient::ErrConnectDB && retErr != IDBClient::UndefinedError) {
        answerPack.status = true;
    }
    else if(answerPack.error.empty()) {
        answerPack.error = "Any ErrStatus";//metaEnum.valueToKey(retErr);
    }

    if(lastUsedTable != "")
        p_dbCli->setTableName(lastUsedTable);

    // ----------- Нужно постепенно отказаться от answerPack и писать напрямую в JSON --
    convertAnswerPackToJson(outJson, nameReqFunc, answerPack);
    // ---------------------------------------------------------

    return outJson;
}

std::vector<std::string> DataProcessingEvent::requestsList()
{
    return m_cSupportRequests;
}

IDBClient::ErrStatus DataProcessingEvent::realTimeArchiveEventCCTV(JSON &jsonPack, const std::string& nameReqFunc,
                                                                   answerOnRequest_t &answerPack)
{
    Timer tmr; tmr.start();

    const std::string subtypeEvents      = Strings::split(Strings::split(nameReqFunc,"-").back(), "@").front();
    const std::string underSubtypeEvents = Strings::split(Strings::split(nameReqFunc,"-").back(), "@").back();
    const std::string supportEventsTable = "registeredanalytics";//"support_events_"+subtypeEvents;

    //p_dbCli->setTableName(supportEventsTable);
    IDBClient::ErrStatus retErr = IDBClient::ErrStatus::ErrSendQuery;
    FieldForFilterEvent fEv;
    std::string timeBegin = "", timeEnd = "";

    getTimeBeginEndEvent(nameReqFunc, jsonPack, timeBegin, timeEnd, fEv.limit, fEv.isThumb);
    fEv.orderDirection = Strings::toUpper(jsonPack.value("order", "asc"));
    fEv.statusView = jsonPack.value("status-view", "*"); // prepare / not send / *
    fEv.timeBegin = timeBegin;
    fEv.timeEnd = timeEnd;

    bool isUnionTypes = false, isDiffTypes = false;
    JSON typeEvents;
    bool retOK = parseTypesInArchiveEvent(jsonPack, subtypeEvents, typeEvents,
                                          isUnionTypes, isDiffTypes);

    if(!retOK) {
        answerPack.status = false;
        answerPack.error = "Неверный types или union-types";
        return IDBClient::UndefinedError;
    }

    std::vector<InfoAboutAnalytic_t> infoAboutAns;
    retErr = getInfoByTypes(supportEventsTable, typeEvents, fEv, infoAboutAns);
    if(retErr == IDBClient::ErrSendQuery) {
        answerPack.errorCode = IDBClient::ErrSendQuery;
        throw dataproc_error("Error Send SQL-query!");
    }
    else if(retErr == IDBClient::NotExists) {
        answerPack.errorCode = IDBClient::NotExists;
        throw dataproc_error("Не найдены аналитики по запрошенным типам!");
    }

    JSON::array_t arrCams;
    if(!jsonPack.contains("cams")) {
        arrCams.push_back("*");
    }
    else if(jsonPack.value("cams", JSON{}).is_array()) {
        arrCams = jsonPack.value("cams", JSON{});
    }

    JSON filtersObject;
    if(jsonPack.contains("filters") && jsonPack["filters"].is_object())
    {
        // Объект фильтров из json-а websocket-запроса
        filtersObject = jsonPack.value("filters", JSON::object_t{});
    }

    std::string needColsStr = "";
    if(jsonPack.contains("need-columns") && jsonPack["need-columns"].is_array() && !jsonPack["need-columns"].empty())
    {
        needColsStr = Strings::join(jsonPack["need-columns"].get<std::vector<std::string>>(), "\",\"");
        if(!needColsStr.empty()) {
            needColsStr = "\""+needColsStr+"\"";
        }
    }

    //LOG(INFO) << "jsonPack[\"need-columns\"]: " << jsonPack["need-columns"];
    LOG(INFO) << "needColsStr: " << needColsStr;

    if(needColsStr.empty())
        needColsStr = "*";

    needColsStr += ", CAST(\"timestamp\" AS text) as timestamp_CAST ";

    if(isDiffTypes)
    {
        std::map<std::string, std::string> filters;
        std::string errText = m_upEventModelCCTV->setFilterGetCCTVEvents(arrCams, filtersObject, fEv, filters);

        if(!errText.empty()) {
            answerPack.error = errText;
            return IDBClient::ErrStatus::NotExists;
        }
        for(auto &itAn : infoAboutAns)
        {
            if(Strings::toLower(subtypeEvents) == "cctv")
            {
                std::string resultName      = jsonPack.value("result-name", "");
                std::string resultCondition = jsonPack.value("result-condition", "");
                if(!resultName.empty() && !resultCondition.empty()) {
                    filters.insert({"\""+resultName+"\" " + resultCondition, ""});
                }
                if(Strings::toLower(underSubtypeEvents) == "asis") {
                    retErr = m_upEventModelCCTV->getDiffCCTVAnalyticEventsAsis(itAn, needColsStr, filters, answerPack);
                }
                else {
                    retErr = m_upEventModelCCTV->getDiffCCTVAnalyticEvents(itAn, needColsStr, filters, answerPack);
                }
            }
//            else if(subtypeEvents.toLower() == "alarm")
//            {
//                if(underSubtypeEvents.toLower() == "asis") {
//                    retErr = EventsModelAlarm::getDiffAlarmAnalyticEventsAsis(itAn, filters, answerPack);
//                }
//                else {
//                    retErr = EventsModelAlarm::getDiffAlarmAnalyticEvents(itAn, filters, answerPack);
//                }
//            }
            if(retErr == IDBClient::ErrStatus::NoError) {
                //choiceSortEvent(nameReqFunc, answerPack);
            }
        }
    }
    else if(isUnionTypes)
    {
        throw dataproc_error("использование union types не поддерживается!");
    }

    LOG(DEBUG) << "Elaps from call, ms : " << tmr.elapsedMilliseconds();
    return retErr;
}

bool DataProcessingEvent::parseTypesInArchiveEvent(const JSON &jsonPack,
                                                   const std::string &subtypeEvents,
                                                   JSON &typeEvents,
                                                   bool &isUnion, bool &isDiff)
{
    if(jsonPack.contains("union-types")) // События, которые надо объединить в одно "суперсобытие"
    {
        isUnion = true;
        if(jsonPack.value("union-types", JSON{}).is_array()) {
            typeEvents= jsonPack.value("union-types", JSON{});
        }
    }
    else if(jsonPack.contains("types")) // События, которые надо рассматривать отдельно
    {
        isDiff = true;
        if(jsonPack.value("types", JSON{}).is_array()) {
            typeEvents = jsonPack.value("types", JSON{});
        }
    }
    else {
        isDiff = true;
        typeEvents.push_back("*");
    }

    return true;
}

bool DataProcessingEvent::getTimeBeginEndEvent(const std::string &recieveRequest, const JSON& jsonPack,
                                               std::string &timeBegin, std::string &timeEnd,
                                               int &limit, bool &isThumb)
{
    if(Strings::startsWith(recieveRequest, "realtime-event") ) {
        timeBegin = jsonPack.value("time", "");
        if(timeBegin.empty())
            return false;
    }
    else if(Strings::startsWith(recieveRequest, "archive-event") ) {
        timeBegin = jsonPack.value("time-begin", "");
    }

    limit   = jsonPack.value("limit", -1);
    isThumb = jsonPack.value("enable-thumb", true);

    //    if(timeBegin.contains(".")) {
    //        timeBegin = timeBegin.split(".").at(0);
    //    }
    //    if(!timeBegin.isEmpty()) {
    //        timeBegin.append(".0");
    //    }

    if(Strings::startsWith(recieveRequest, "realtime-event") ) {
        timeEnd = jsonPack.value("time-end", "");
    }
    else if(Strings::startsWith(recieveRequest, "archive-event") ) {
        timeEnd = jsonPack.value("time-end", "");
    }

    if(timeEnd.empty() && timeBegin.empty())
        return false;

    return true;
}

IDBClient::ErrStatus DataProcessingEvent::getInfoByTypes(const std::string &analyticsListTable,
                                                         const JSON::array_t &typeEvents,
                                                         FieldForFilterEvent &fEv,
                                                         std::vector<InfoAboutAnalytic_t> &infoAboutAns)
{
    p_dbCli->setTableName(analyticsListTable);
    auto retErr = IDBClient::ErrStatus::UndefinedError;
    bool needAllSupportEvent = false;
    if(typeEvents.size() > 0) {
        if(typeEvents.at(0).is_string()) {
            if(typeEvents.at(0) == "*") {
                needAllSupportEvent = true;
                retErr = m_upEventModelCCTV->fillInfoAboutAllAnalytics(infoAboutAns, fEv.tables);
            }
        }
    }

    if(!needAllSupportEvent) {
        for(auto &itType : typeEvents) // Обход по suppot_events_*, для получения свойств доступных аналитик
        {
            InfoAboutAnalytic_t tmpInfo;
            retErr = m_upEventModelCCTV->fillInfoAboutAnalytic(itType, tmpInfo);
            if(retErr != IDBClient::ErrStatus::NotExists && tmpInfo.type > 0) {
                infoAboutAns.push_back(tmpInfo);
                fEv.tables.insert({tmpInfo.description, tmpInfo.nameTable});
            }
        }
    }
    return retErr;
}
