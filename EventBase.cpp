#include "EventBase.h"
#include "Utils/mstrings.h"
#include "loggerpp/include/LoggerPP.h"

EventBase::EventBase(IDBClient *pdbCli)
{
    p_dbCli = pdbCli;
}

std::string EventBase::fillNodesStrForEvent(const JSON::array_t &cams, FieldForFilterEvent &fEv,
                                         std::string &pidNodesStr, std::string &sensorIdsStr)
{
    std::string errText = "";
    for(const auto &itCamObj : cams) {

        if(itCamObj.is_string()) {
            if(itCamObj == "*") {
                pidNodesStr = "";
                sensorIdsStr = "";
                break;
            }
        }

        fEv.pidNode     = itCamObj.value("camera-id",-1);
        fEv.sensorToken = itCamObj.value("sensor-id", "");

        if(fEv.pidNode < 0) {
            errText = "Field camera-id not found in request Json";
            return errText;
        }
        else {
            pidNodesStr += std::to_string(fEv.pidNode) + ",";
        }

        if(!fEv.sensorToken.empty()){
            sensorIdsStr += "'"+fEv.sensorToken + "',";
        }
    }
    if(!pidNodesStr.empty()) {
        Strings::chop(pidNodesStr,1);
        pidNodesStr = "("+pidNodesStr+")";
    }
    if(!sensorIdsStr.empty()) {
        Strings::chop(sensorIdsStr,1);
        sensorIdsStr = "("+sensorIdsStr+")";
    }

    return errText;
}

IDBClient::ErrStatus EventBase::fillInfoAboutAnalytic(int32_t numType, InfoAboutAnalytic_t &infoAboutAnalytic)
{
    std::vector<JSON> tmpDataMaps;
    bool stateBool = false;

    std::map<std::string, std::string> tmpFilters;
    tmpFilters["idx"] = std::to_string(numType);

    auto retErr = p_dbCli->getFilterRow(tmpDataMaps, tmpFilters);

    LOG(DEBUG) << "tmpDataMaps.empty() ? " << tmpDataMaps.empty();

    if(retErr == IDBClient::ErrSendQuery) {
        return retErr;
    }
    else if(tmpDataMaps.empty()) {
        return IDBClient::ErrStatus::NotExists;
    }

    if(tmpDataMaps.at(0).contains("enable") && tmpDataMaps.at(0)["enable"].is_string()) {
        stateBool   = tmpDataMaps.at(0).value("enable", "false") == "false" ? false : true;
    }
    else if(tmpDataMaps.at(0).contains("enable") && tmpDataMaps.at(0)["enable"].is_boolean()) {
        stateBool   = tmpDataMaps.at(0).value("enable", false);
    }
    else if(tmpDataMaps.at(0).contains("enable") && tmpDataMaps.at(0)["enable"].is_number()) {
        stateBool   = tmpDataMaps.at(0).value("enable", 0);
    }

    if(stateBool != true) {
        return IDBClient::ErrStatus::NotExists;
    }
    else
    {
        infoAboutAnalytic.description = tmpDataMaps.at(0).value("module_name", "");
        infoAboutAnalytic.stateBool = stateBool;
        std::string anType = "none", anSubType = "";
        if(tmpDataMaps.at(0).contains("analytics_type") && tmpDataMaps.at(0)["analytics_type"].is_string())
            anType = tmpDataMaps.at(0).value("analytics_type", "");

        if(tmpDataMaps.at(0).contains("analytics_sub_type") && tmpDataMaps.at(0)["analytics_sub_type"].is_string())
            anSubType = tmpDataMaps.at(0).value("analytics_sub_type", "");

        infoAboutAnalytic.nameTable = anType+anSubType;
        infoAboutAnalytic.type = tmpDataMaps.at(0).value("idx", 0);

        if(infoAboutAnalytic.nameTable.empty())
        {
            LOG(WARNING) << "Пустое имя таблицы у аналитики: " << infoAboutAnalytic.description;
            return IDBClient::ErrStatus::NotExists;
        }

        LOG(DEBUG) << infoAboutAnalytic;
    }

    return retErr;
}

IDBClient::ErrStatus EventBase::fillInfoAboutAllAnalytics(std::vector<InfoAboutAnalytic_t> &infoAboutAllAnalytics,
                                                          std::map<std::string, std::string> &tables)
{
    std::vector<JSON> tmpDataMaps;

    std::map<std::string, std::string> tmpFilters;
    tmpFilters.insert({"enable", "1"});

    auto retErrParse = IDBClient::ErrStatus::NotExists;

    auto retErrSQL = p_dbCli->getFilterRow(tmpDataMaps, tmpFilters);  // TODO изменить на state_bool
    if(retErrSQL == IDBClient::ErrSendQuery) {
        return retErrSQL;
    }
    else if(tmpDataMaps.empty()) {
        retErrSQL = IDBClient::ErrStatus::NotExists;
    }
    else {
        for(auto &itOne : tmpDataMaps)
        {
            InfoAboutAnalytic_t infoOne;
            infoOne.description = itOne.value("module_name", "");
            if(itOne.contains("enable") && itOne["enable"].is_string()) {
                infoOne.stateBool   = itOne.value("enable", "false") == "false" ? false : true;
            }
            else if(itOne.contains("enable") && itOne["enable"].is_boolean()) {
                infoOne.stateBool   = itOne.value("enable", false);
            }
            else if(itOne.contains("enable") && itOne["enable"].is_number()) {
                infoOne.stateBool   = itOne.value("enable", 0);
            }

            infoOne.nameTable   = itOne.value("analytics_type", "")+itOne.value("analytics_sub_type", "");
            infoOne.type        = itOne.value("idx", -1);

            if(infoOne.nameTable.empty())
            {
                LOG(WARNING) << "Пустое имя таблицы у аналитики: " << infoOne.description;
                retErrParse = IDBClient::ErrStatus::NotExists;
                continue;
            }
            else retErrParse = IDBClient::ErrStatus::NoError;

            LOG(DEBUG) << infoOne ;

            tables.insert({infoOne.description, infoOne.nameTable});
            infoAboutAllAnalytics.push_back(infoOne);
        }
    }

    auto retErr = (retErrParse == IDBClient::NoError) ? retErrSQL : retErrParse;

    return retErrSQL;
}
