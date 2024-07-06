#ifndef DATAPROCESSINGEVENT_H
#define DATAPROCESSINGEVENT_H

#include "DataProcessingBasic.h"
//#include "IDataProcessing.h"
#include "EventsModelCCTV.h"

class DataProcessingEvent : public IDataProcessing
{
    friend class DataProcessingVMS;
public:
    DataProcessingEvent(IDBClient* pdbCli);
    virtual ~DataProcessingEvent();

    virtual JSON doWorkByRequest(JSON &jsonPack, const std::string &urlSender,
                                 const std::string& nameReqFunc);

protected:
    virtual std::vector<std::string> requestsList();

private:
    IDBClient::ErrStatus realTimeArchiveEventCCTV(JSON &jsonPack, const std::string &nameReqFunc,
                                                  answerOnRequest_t &answerPack);

    DataProcessingBasic *m_pBasicProc = nullptr;
    IDBClient* p_dbCli = nullptr;

    std::unique_ptr<EventsModelCCTV> m_upEventModelCCTV;

    const std::vector<std::string> m_cSupportRequests =
    {
        "realtime-event-cctv",
        "archive-event-cctv",
        "realtime-event-cctv@asis",
        "archive-event-cctv@asis"
        };

    bool parseTypesInArchiveEvent(const JSON &jsonPack, const std::string &subtypeEvents,
                                  JSON &typeEvents, bool &isUnion, bool &isDiff);

    bool getTimeBeginEndEvent(const std::string &recieveRequest, const JSON &jsonPack,
                              std::string &timeBegin, std::string &timeEnd,
                              int &limit, bool &isThumb);

    IDBClient::ErrStatus getInfoByTypes(const std::string &analyticsListTable,
                                        const JSON::array_t &typeEvents, FieldForFilterEvent &fEv,
                                        std::vector<InfoAboutAnalytic_t> &infoAboutAns);
};

#endif // DATAPROCESSINGEVENT_H
