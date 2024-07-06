#ifndef DATAPROCESSING_H
#define DATAPROCESSING_H

#include "json/TBaseJsonWork.h"
#include "IDbClient.h"

#include "DataProcessingAnalytic.h"
#include "DataProcessingEvent.h"
#include "DataProcessingBasic.h"
#include "DataProcessingVMS.h"

class RequestParser
{
public:
    RequestParser(IDBClient* pDbClient);
    ~RequestParser();

    void slotRequestProc(JSON &inJson, std::string urlSender, JSON &outJson);

private:

    DataProcessingBasic    *m_pBasicProc     = nullptr;
    DataProcessingAnalytic *m_pAnalyticProc  = nullptr;
    DataProcessingEvent    *m_pEventProc     = nullptr;
    DataProcessingVMS      *m_pVMSProc       = nullptr;

    IDBClient *p_db = nullptr;

    const std::string c_MagicWord = "mtp";
    JSON eraseBase64Field(const JSON &inJson);
};

#endif // DATAPROCESSING_H
