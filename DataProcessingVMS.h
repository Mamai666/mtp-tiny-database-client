#ifndef DATAPROCESSINGVMS_H
#define DATAPROCESSINGVMS_H

#include "DataProcessingBasic.h"
#include "DataProcessingEvent.h"

class DataProcessingVMS : public IDataProcessing
{
public:
    DataProcessingVMS(IDBClient* pdbCli);
    virtual ~DataProcessingVMS();

    virtual JSON doWorkByRequest(JSON &jsonPack, const std::string &urlSender,
                                 const std::string& nameReqFunc);

protected:
    virtual std::vector<std::string> requestsList();

private:
    answerOnRequest_t    readArchive(JSON &jsonPack);
    answerOnRequest_t    archiveTimeline(JSON &jsonPack);

    DataProcessingBasic *m_pBasicProc = nullptr;
    IDBClient* p_dbCli = nullptr;

    DataProcessingEvent *m_pEventProc = nullptr;

    const std::vector<std::string> m_cSupportRequests =
    {
        "read-archive",
        "archive-timeline-cctv"
    };
};

#endif // DATAPROCESSINGVMS_H
