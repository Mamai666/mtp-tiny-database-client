#ifndef EVENTBASE_H
#define EVENTBASE_H

#include "IDbClient.h"
#include <string>
#include <map>
#include <vector>
#include <inttypes.h>
#include <json/TBaseJsonWork.h>

struct FieldForFilterEvent
{
    int pidNode = 0;
    std::string sensorToken = "";
    int limit = -1;
    bool isThumb = true;
    std::string orderDirection = "ASC";
    std::string timeBegin;
    std::string timeEnd;
    std::string statusView = "*";

    std::map<std::string, std::string> tables; // key = псевдонимТаблицы , value = название_таблицы
    std::map<std::string, std::string> needColumns; // key = псевдонимТаблицы , value = нужные столбцы
};

struct InfoAboutAnalytic_t
{
    int32_t type = 0;
    std::string description = "";
    //std::string state;
    bool stateBool = false;
    std::string nameTable;

    friend std::ostream& operator<< (std::ostream &out, const InfoAboutAnalytic_t &res)
    {
        out << "\n Print InfoAboutAnalytic:\n";

        out  << "type == " << res.type << " ;\n"
            << "description == " << res.description << " ;\n"
            << "stateBool == " << res.stateBool << " ;\n"
            << "nameTable == " << res.nameTable << " ;\n";

        return out;
    }

};

class EventBase
{
public:
    EventBase(IDBClient* pdbCli);

    std::string fillNodesStrForEvent(const JSON::array_t &cams, FieldForFilterEvent &fEv,
                                        std::string &pidNodesStr, std::string &sensorIdsStr);

    IDBClient::ErrStatus fillInfoAboutAnalytic(int32_t numType, InfoAboutAnalytic_t &infoAboutAnalytic);
    IDBClient::ErrStatus fillInfoAboutAllAnalytics(std::vector<InfoAboutAnalytic_t> &infoAboutAllAnalytics,
                                                          std::map<std::string, std::string> &tables);
protected:
    IDBClient* p_dbCli = nullptr;
};

#endif // EVENTBASE_H
