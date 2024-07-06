#include "RequestParser.h"
#include <string>
#include "loggerpp/include/LoggerPP.h"
#include "Utils/mstrings.h"

RequestParser::RequestParser(IDBClient* pDbClient)
{
    p_db = pDbClient;
    m_pBasicProc     = new DataProcessingBasic(pDbClient);
    m_pAnalyticProc  = new DataProcessingAnalytic(pDbClient);
    m_pEventProc     = new DataProcessingEvent(pDbClient);
    m_pVMSProc       = new DataProcessingVMS(pDbClient);
}

RequestParser::~RequestParser()
{
    if(m_pBasicProc)
        delete m_pBasicProc;

    if(m_pAnalyticProc)
        delete m_pAnalyticProc;

    if(m_pEventProc)
        delete m_pEventProc;
}

JSON RequestParser::eraseBase64Field(const JSON &inJson)
{
    JSON viewJson = inJson;
    if(inJson.contains("crop_thumb")) {
        viewJson.erase("crop_thumb");
    }
    if(inJson.contains("screen_thumb")) {
        viewJson.erase("screen_thumb");
    }
    if(inJson.contains("zbase64_img")) {
        viewJson.erase("zbase64_img");
    }
    return viewJson;
}

void RequestParser::slotRequestProc(JSON &inJson, std::string urlSender, JSON &outJson)
{
    std::string request = "";
    std::string error = "";

    if(inJson.contains("query") || inJson.contains("request"))
    {
        request = inJson.contains("request") ? inJson.value("request", "") : inJson.value("query", "");
        std::string method = inJson.value("method", "");

        LOG(INFO) << "******** Request: " << request << "***********";
        LOG(DEBUG) << "Json: \n" << eraseBase64Field(inJson).dump(4) << "\n";

        std::string uniqName  = inJson.value("unique-name", "");
        std::string uniqValue = inJson.value("unique-value", "");

        std::string nameReqFunc = method.empty() ? request : method;

        std::string reqDbTable = inJson.value("table", "");
        if(!reqDbTable.empty())
        {
            p_db->setTableName(reqDbTable);
            LOG(DEBUG) << "reqDbTable == " << reqDbTable;
        }

        if(nameReqFunc == "heartbeat")
        {
            outJson["my-urlSender"] = urlSender;
            outJson["status"]       = true;
        }
        else if(m_pBasicProc->hasRequest(nameReqFunc))
        {
            outJson = m_pBasicProc->doWorkByRequest(inJson, urlSender, nameReqFunc);
        }
        else if(m_pAnalyticProc->hasRequest(nameReqFunc))
        {
            outJson = m_pAnalyticProc->doWorkByRequest(inJson, urlSender, nameReqFunc);
        }
        else if(m_pEventProc->hasRequest(nameReqFunc))
        {
            outJson = m_pEventProc->doWorkByRequest(inJson, urlSender, nameReqFunc);
        }
        else if(m_pVMSProc->hasRequest(nameReqFunc))
        {
            outJson = m_pVMSProc->doWorkByRequest(inJson, urlSender, nameReqFunc);
        }
        else error = "Не найден запрос "+nameReqFunc;

        if(!uniqValue.empty())
            outJson[uniqName] = uniqValue;

        if(!reqDbTable.empty())
            outJson["table"]  = reqDbTable;
    }
    else
    {
        error = "Json not contains query or request field! Drop";
    }

    LOG(INFO) << "Result status: " << (outJson.is_null() ? false : outJson.value("status", false));

    if(inJson.contains(c_MagicWord))
    {
        outJson[c_MagicWord] = inJson.value(c_MagicWord, "");
    }

    outJson["answer"] = request;

    if(error != "")
    {
        LOG(ERROR) << "Error: " << error;
        outJson["error"]  = error;
        outJson["status"] = false;
    }

    if(!outJson.contains("status"))
    {
        LOG(WARNING) << "Пустое поле status!";
        outJson["status"] = false;
    }
}

//void RequestParser::slotRequestProcOld(JSON &inJson, std::string urlSender, JSON &outJson)
//{
//    answerOnRequest_t answerPack;
//    auto &request = answerPack.request;
//    answerPack.status = false;

//    if(inJson.contains("query") || inJson.contains("request"))
//    {
//        request = inJson.contains("request") ? inJson.value("request", "") : inJson.value("query", "");
//        LOG(INFO) << "******** Request: " << request << "***********";
//        LOG(DEBUG) << "Json: \n" << eraseBase64Field(inJson).dump(4) << "\n";

////        std::string saveToken = jsonPack.value("token", "");
////        if(request.startsWith("authentication"))
////        {
////            answerPack = m_auth.authentication(jsonPack, request);
////            m_authorisationList[urlSender] = answerPack.status;
////        }

////        if(!isMayBeAuthorisation(urlSender) && !m_nonAuthRequestList.contains(request) ) // Клиент не может быть авторизован
////        {
////            answerPack.error = "Вы не можете быть авторизованы! Проверьте реквизиты!";
////        }
////        else if(!request.startsWith("authentication"))
//        if(request.find("authentication") == std::string::npos)
//        {
//            LOG(INFO) << "Вы авторизованны! Токен авторизации подтвержден!";

//            std::string dbTable = "";
//            if(inJson.contains("table")) {
//                dbTable = inJson.value("table", "");
//                LOG(DEBUG) << "dbTable == " << dbTable;
//            }

//            if(dbTable!= "") {
//                p_db->setTableName(dbTable);
//            }

//            if(request == "heartbeat")
//            {
//                outJson["my-urlSender"] = urlSender;
//                answerPack.status = true;
//            }
//            else if(request == "addNewRecord" || inJson.value("method", "") == "addNewRecord")
//            {
//                //uniqName = inJson.value("toWrite", JSON{}).find("unique-name").value();
//                //uniqValue = inJson.value("toWrite", JSON{}).find("unique-value").value();
//                answerPack.status = m_pBaseProc->addNewRecord(inJson, false, true);

//                outJson["table"]  = dbTable;
//            }
//            else if(requestLowCase.find("system") != std::string::npos) {
//               // answerPack = m_IVMS.callIVMS_prepareSystem(inJson, request);
//            }
//            else if(dbTable == "pg_database" || dbTable == "pg_roles" ||
//                     request == "getLastConnectionInfo" ||
//                     request == "getListUsers" ||
//                     request == "getListDB" ||
//                     request == "getListDBUsers" ||
//                     request == "getListOwnersDB"
//                     )
//            {
//               // answerPack = m_IVMS.callIVMS_other(inJson, request);
//            }
//            else if(request.find("Node") != std::string::npos ||
//                    request.find("Layout") != std::string::npos ||
//                    Strings::startsWith(request, "getAll")) {
//               // answerPack = m_IVMS.callIVMS_intoSystem(inJson, request);
//            }
//            else if(request.find("metaData2Db") != std::string::npos) {
//                answerPack = m_pAnalyticProc->callWithVideoAnalytics(inJson, request);
//            }
//            else if(Strings::startsWith(request, "getEmbeddingInfo")) {
//                answerPack = m_pAnalyticProc->getEmbeddingInfo(inJson, request);
//            }
//            else if(Strings::startsWith(request, "analyticsRegister")) {
//                answerPack = m_pAnalyticProc->registerAnalytics(inJson, request, urlSender);
//            }
//            else if(request == "update")
//            {
//                if(inJson.contains("unique-name"))
//                {
//                    uniqName = inJson.value("unique-name", "");
//                    uniqValue = inJson.value("unique-value", "");

//                    LOG(INFO) << "For row with " << uniqName << " == " << uniqValue;
//                    answerPack.status = m_pBaseProc->updateProc(inJson, uniqName, uniqValue);
//                }
//                else
//                {
//                    answerPack.status = false;
//                    answerPack.error = "Json not contains unique-name!";
//                }
//            }
//            else {
//                //answerPack = m_ARM.callWithMonitor(jsonPack, request);
//            }

//            std::vector<JSON> dataArr;
//            if(!answerPack.dataMaps.empty())
//            {
//                dataArr.resize(answerPack.dataMaps.size());
//                for(int i = 0; i < answerPack.dataMaps.size(); ++i)
//                {
//                    for(const auto &itV : answerPack.dataMaps[i])
//                    {
//                        JSON dataObj = itV; //JSON::from_msgpack( QJsonObject::fromVariantMap(itV);
//                        dataArr[i].push_back(dataObj);
//                    }
//                }
//            }
//            if(!answerPack.timeLastUPD.empty()) {
//                outJson["time-last"] = answerPack.timeLastUPD;
//            }
//            if(Strings::startsWith(request, "archive-timeline")) {
//                outJson["available-ranges"]   = dataArr.at(0);
//                outJson["ranges-with-events"] = dataArr.at(1);
//            }
//            else if(Strings::startsWith(request, "getEmbedding")) {
//                outJson["embeddings"] = dataArr.at(0);
//            }
//            else {
//                outJson["data"] = dataArr.at(0);
//            }

//            JSON listArray;
//            if(!answerPack.dataList.empty())
//            {
//                listArray = answerPack.dataList;
//                outJson["items"] = listArray;
//            }
//        }

//        if(!uniqValue.empty())
//            outJson[uniqName] = uniqValue;
//    }
//    else {
//        answerPack.error = "Json not contains query or request field! Drop";
//    }

//    LOG(INFO) << "Result status: " << answerPack.status;

//    if(inJson.contains(c_MagicWord))
//    {
//        outJson[c_MagicWord] = inJson.value(c_MagicWord, "");
//    }

//    outJson["answer"] = answerPack.request;
//    if(answerPack.error != "") {
//        LOG(ERROR) << "Error: " << answerPack.error;
//        outJson["error"] = answerPack.error;
//        answerPack.status = false;
//    }
//    outJson["status"] = answerPack.status;
//    //emit requestComplete(mainObj, urlSender);
//}
