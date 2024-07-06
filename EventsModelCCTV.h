#ifndef EVENTSMODELCCTV_H
#define EVENTSMODELCCTV_H

#include "DataProcessingBasic.h"
#include "EventBase.h"

class EventsModelCCTV : public EventBase
{
public:
    EventsModelCCTV(IDBClient* pdbCli);

    std::string setFilterGetCCTVEvents(const JSON::array_t &cams, JSON &extFiltersObj,
                                       FieldForFilterEvent &fEv,
                                       std::map<std::string, std::string> &outFilters);

    IDBClient::ErrStatus getDiffCCTVAnalyticEventsAsis(const InfoAboutAnalytic_t &infoAnalytic, std::string &needColumns,
                                                       std::map<std::string, std::string> &filters,
                                                       answerOnRequest_t &answerPack);
    IDBClient::ErrStatus getDiffCCTVAnalyticEvents(const InfoAboutAnalytic_t &infoAnalytic, std::string &needColumns,
                                                   std::map<std::string, std::string> &filters,
                                                   answerOnRequest_t &answerPack);
};

#endif // EVENTSMODELCCTV_H
