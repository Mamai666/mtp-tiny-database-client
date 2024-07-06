#ifndef DBMAINMANAGER_H
#define DBMAINMANAGER_H

#include "configmgr/include/ConfigManager.h"
#include "IDbClient.h"
#include <thread>

//#include "watchdog_worker/IWatchDogClient.h"
#include "watchdog_worker/WatchDogWorker.h"

#include "watchdog_worker/systemd_client/SystemdClient.h"
#include "watchdog_worker/modmon_client/ModMonClient.h"

#include "message_transport/include/BaseTransportServer.h"

#include "RequestParser.h"

struct DBClientSettings
{
    std::map<std::string, BrokerDescription_t> inChannels;
    std::map<std::string, BrokerDescription_t> outChannels;

    WatchDogSettings_t watchDog;

    DBSettings db;
    bool isUseAuthentication;
};

class DBMainManager
{
public:
    DBMainManager(IDBClient* dbClient, ConfigManager* pConfManager, bool *isWorking);
    ~DBMainManager();

    void startMonitoringConfig();
    void startProcess();

private:
    IDBClient     *p_dbClient    = nullptr;
    ConfigManager *p_confManager = nullptr;

    BaseTransportServer *m_pTransportServer  = nullptr;

    IWatchDogClient     *m_pWdClient        = nullptr;
    WatchDogWorker      *m_pWatchDogWorker  = nullptr;

    RequestParser       *m_pReqParser        = nullptr;

    std::thread          m_thrConfigMgr;
    DBClientSettings     m_newParams;
    DBClientSettings     m_oldParams;

    bool          *p_isWorking    = nullptr;

private:
    bool parseConfig(const ConfigManager *cfg);

    void checkInput(DBClientSettings &sNew, DBClientSettings &sOld);

    void slotRequestProc(JSON jsonPack, std::string urlSender);
    void checkAlive();
};

#endif // MAINMANAGER_H
