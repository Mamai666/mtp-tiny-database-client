#ifndef DATAPROCESSINGANALYTIC_H
#define DATAPROCESSINGANALYTIC_H

#include "DataProcessingBasic.h"
#include "IDataProcessing.h"

class DataProcessingAnalytic : public IDataProcessing
{
public:
    DataProcessingAnalytic(IDBClient* pdbCli);
    virtual ~DataProcessingAnalytic();

    virtual JSON doWorkByRequest(JSON &jsonPack, const std::string &urlSender,
                                 const std::string& nameReqFunc);

protected:
    virtual std::vector<std::string> requestsList();

private:
    answerOnRequest_t    callWithVideoAnalytics(JSON &jsonPack);
    IDBClient::ErrStatus updateEmbeddingInfo(JSON &jsonPack);
    answerOnRequest_t    getEmbeddingInfo(JSON &jsonPack);

    answerOnRequest_t    registerAnalytics(JSON &jsonPack, const std::string &urlSender);

    DataProcessingBasic *m_pBasicProc = nullptr;
    IDBClient* p_dbCli = nullptr;

    const std::vector<std::string> m_cSupportRequests =
    {
        "metaData2Db",
        "getEmbeddingInfo",
        "analyticsRegister"
    };

};

#endif // DATAPROCESSINGANALYTIC_H
