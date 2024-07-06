#include "DBMainManager.h"
#include "loggerpp/include/LoggerPP.h"

using namespace std::chrono_literals;

DBMainManager::DBMainManager(IDBClient *dbClient, ConfigManager *pConfManager, bool *isWorking)
{
    if(dbClient == nullptr) {
        LOG(FATAL) << "Ошибка создания DBMainManager: dbClient == nullptr";
        exit(-1);
    }
    p_isWorking    = isWorking;
    p_dbClient    = dbClient;
    p_confManager = pConfManager;

    m_pReqParser = new RequestParser(p_dbClient);
}

DBMainManager::~DBMainManager()
{
    if(m_pTransportServer)
        delete m_pTransportServer;

    if(m_pReqParser)
        delete m_pReqParser;

    if(m_pWatchDogWorker)
    {
        if (!m_pWatchDogWorker->stop()) {
            LOG(ERROR) << "Не удалось отключиться от диспетчера" << std::endl;
        }
        else {
            LOG(INFO) << "Стоп общения с диспетчером!";
        }

        delete m_pWatchDogWorker;
    }

    if(m_pWdClient)
        delete m_pWdClient;
}

void DBMainManager::startMonitoringConfig()
{
    bool retOK = p_confManager->loadInputConfig();
    if(!retOK) {
        LOG(ERROR) << "Ошибка начального чтения конфиг файла!";
        exit(-1);
    }

    retOK = parseConfig(p_confManager);
    if(!retOK) {
        LOG(ERROR) << "Ошибка начального парсинга конфиг файла!";
        exit(-1);
    }

    //************************ НАСТРОЙКА СЛЕЖЕНИЯ ЗА КОНФИГАМИ ****************
    m_thrConfigMgr = std::thread(&ConfigManager::startMonitor, p_confManager, p_isWorking, [this]()->void
    {
        JSON configJson = p_confManager->getConfig();
        if(parseConfig(p_confManager))
        {
            checkInput(m_newParams, m_oldParams);

            p_confManager->saveOutConfig();
        }
    });
}

void DBMainManager::startProcess()
{
    m_oldParams = m_newParams;

    try
    {
        auto &consumerOfWD = m_oldParams.watchDog.consumer;
        if(consumerOfWD == "systemd" || consumerOfWD == "Systemd")
        {
            m_pWdClient = new SystemdClient(m_oldParams.watchDog,
                                            p_confManager->binaryName(),
                                            p_confManager->inConfigPath(), p_isWorking);
        }
        else if(consumerOfWD == "Dispatcher" || consumerOfWD == "Monitor")
        {
            m_pWdClient = new ModMonClient(m_oldParams.watchDog,
                                           p_confManager->binaryName(),
                                           p_confManager->inConfigPath(), p_isWorking);
        }

        if(m_pWdClient)
        {
            m_pWatchDogWorker = new WatchDogWorker(m_pWdClient);
            m_pWatchDogWorker->setItWorks(p_isWorking);

            if(!m_pWatchDogWorker->start()) {
                LOG(ERROR) << "Не удалось подключиться к диспетчеру: "
                           << m_pWatchDogWorker->lastError(true)
                           << std::endl;

                *p_isWorking = false;

                std::this_thread::sleep_for(std::chrono::milliseconds(500));

                if(m_pWatchDogWorker)
                {
                    delete m_pWatchDogWorker;
                    m_pWatchDogWorker = nullptr;
                }

                if(m_pWdClient)
                {
                    delete m_pWdClient;
                    m_pWdClient = nullptr;
                }

                if(m_thrConfigMgr.joinable())
                    m_thrConfigMgr.join();

                return;
            }
            else {
                LOG(INFO) << "Старт общения с диспетчером!";
                //m_pHikCapture->setP_watchDog(m_pWatchDogWorker);
            }
        }
        else
        {
            LOG(WARNING) << "Внимание: запуск модуля без мониторинга!";
        }

    }
    catch(...)
    {
        LOG(ERROR) << "Поймано исключение при инициализации класса мониторинга!";
        *p_isWorking = false;
        return;
    }

    p_dbClient->connect(m_oldParams.db);

    m_pTransportServer = BaseTransportServer::createBroker(m_oldParams.inChannels.at("Server"));
    if(m_pTransportServer == nullptr)
    {
        LOG(ERROR) << "Критическая ошибка: невозможно создать m_transportServer! Выход..";
        *p_isWorking = false;
        return;
    }

    m_pTransportServer->setFCallBackResponse([&, this](uint8_t* reqNew, size_t reqSize, std::string sender) -> void
    {
        JSON jsReq;
        try
        {
            jsReq = JSON::parse(reqNew);
        }
        catch (JSON::exception& e)
        {
            LOG(ERROR) << "Ошибка парсинга принятого json от "<<sender<<": " << e.what()
                << "\n Сообщение: " << reqNew <<"\n";
            return;
        }

        JSON resultAnsw;
        m_pReqParser->slotRequestProc(jsReq, sender, resultAnsw);
        std::string strAnsw = resultAnsw.dump(4);
        m_pTransportServer->sendAnswer((uint8_t*)strAnsw.c_str(), strAnsw.size(), sender);
    });

    m_pTransportServer->setP_GlobalIsWork(p_isWorking);

    std::thread thrRunTransportServer(&BaseTransportServer::run, m_pTransportServer);
    std::thread thrCheckAlive(&DBMainManager::checkAlive, this);

    if(thrRunTransportServer.joinable())
        thrRunTransportServer.join();

    if(thrCheckAlive.joinable())
        thrCheckAlive.join();

    if(m_thrConfigMgr.joinable())
        m_thrConfigMgr.join();
}

bool DBMainManager::parseConfig(const ConfigManager *cfg)
{
    try
    {
        if(cfg->isContainParam("In"))
        {
            JSON inObj = cfg->getParamValue<JSON>("/In/");
            if(inObj.is_null())
            {
                LOG(WARNING) << "In == null!";
            }
            for(auto &it : inObj.items())
            {
                //LOG(DEBUG) << "Key: " << it.key();
                BrokerDescription_t inItem;
                inItem.deserializeFromJson(it.value());
                m_newParams.inChannels[it.key()] = inItem;
            }
        }

        m_newParams.db.deserializeFromJson(cfg->getParamValue<JSON>("/DB/"));

        m_newParams.watchDog.timeWaitConnectMs  = cfg->getParamValue<uint32_t>("/Dispatcher/TimeWaitConnect");
        m_newParams.watchDog.delaySendMessageMs = cfg->getParamValue<uint32_t>("/Dispatcher/DelaySendMessage");
        m_newParams.watchDog.consumer           = cfg->getParamValue<std::string>("/Dispatcher/Consumer");

        m_newParams.watchDog.broker.deserializeFromJson(cfg->getParamValue<JSON>("/Dispatcher/Broker/"));

        try
        {
            if(cfg->getConfig().contains("ForDebug"))
            {
            }
        }
        catch (config_error &ec) {
            LOG(WARNING) << "ForDebug Except: " << ec.what();
        }

        return true;
    }
    catch (config_error &ec) {
        LOG(WARNING) << "ConfigParse Except: " << ec.what();
    }
    catch (std::exception &ec) {
        LOG(WARNING) << "Std::Except: " << ec.what();
    }
    catch (...) {
        LOG(ERROR) << "Словили исключение в parseConfig!";
    }
    return false;
}

void DBMainManager::checkInput(DBClientSettings &sNew, DBClientSettings &sOld)
{
    LOG(WARNING) << "Пока не реализовано!\n";
}

void DBMainManager::checkAlive()
{
    while(*p_isWorking)
    {
        if(m_pTransportServer)
        {
            auto currStatusTransport = m_pTransportServer->getStatus();
            auto statusForWD = WatchDog::Statuses::Status_OK;
            if(currStatusTransport != TransportState::STATUS_OK)
            {
                LOG(ERROR) << "Статус: " << static_cast<int>(currStatusTransport);
                statusForWD = WatchDog::Statuses::Status_ErrAsyncTimeOut;
            }
            if(m_pWatchDogWorker)
            {
                m_pWatchDogWorker->notify(statusForWD);
            }
        }
        else
        {
            LOG(ERROR) << "m_pTransportServer is nullptr!";
            break;
        }
        std::this_thread::sleep_for(500ms);
    }
}

