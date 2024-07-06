#include <iostream>
#include <unistd.h>

#include "DBMainManager.h"

#include "PostgreSQLClient.h"
#include "SQLite3Client.h"

#include <iostream>
#include <signal.h>
#include "Utils/files.h"
#include "Utils/time_convert.h"

#include <thread>

#include "configmgr/include/ConfigManager.h"
#include "loggerpp/include/LoggerPP.h"
#define V_DATE_OF_BUILD __DATE__ " " __TIME__
//INITIALIZE_EASYLOGGINGPP

static bool s_isWorking = false;

static void sigStop(int)
{
    static int cntRepeat = 0;
    if(++cntRepeat > 3)
    {
        exit(-1);
    }
    LOG(DEBUG) << "ОШИБКА: Прилетел стоп-сигнал! \n";
    s_isWorking = false;
}

static void sigPipe(int)
{
    LOG(DEBUG) << "ОШИБКА: Потеря связи с pipe! \n";
    // s_isWorking = false;
}

int main(int argc, char *argv[])
{
    signal(SIGINT, &sigStop);
    signal(SIGTERM, &sigStop);

#if !defined(use_POSTGRESQL) && !defined(use_SQLITE3)
#   error Не задан тип используемого клиента БД при сборке! use_POSTGRESQL или use_SQLITE3
#endif

    std::string dbClient="";
#ifdef use_SQLITE3
    dbClient = "SQLite3";
#elif use_POSTGRESQL
    dbClient = "PostgreSQL";
#endif

    if((argc == 2) && (std::string(argv[1]) == "-v") ) {
        std::cerr << "Version Date: " << V_DATE_OF_BUILD
                  << ", dbClient: " << dbClient << std::endl;
        exit(0);
    }
    else if(argc < 3)
    {
        LOG(DEBUG) << "\n Low arguments number. Must be >=3! \n ";
        return 0;
    }
    s_isWorking = true;

    std::string PWD = Files::dirname(argv[0]);

    const std::string in_configPath  = std::string(argv[1]);
    const std::string constraintPath = std::string(argv[2]);

    /**
     * @brief loggerPP
     * loggerConfPath - путь к конфигу логгера, для его тонкой настройки
     * suffixForLog - суффикс, который добавляется в конец имени лог-файла
     * @return
     */

    std::string sufx = ConfigManager::getSuffixFromConfig(in_configPath);
    LoggerPP *loggerPP = new LoggerPP("../configurations/logger.conf", sufx);

    /**
     * @brief logStartThread - старт контроля файлов лога (ротация, очистка)
     */
    std::thread logStartThread( &LoggerPP::run, loggerPP, &s_isWorking);

    /**
     * @brief confMgr
     * in_configPath - путь к входному конфигу модуля
     * constraintPath - путь к констрейну модуля
     * binaryName - имя бинарника
     * @return
     */
    ConfigManager *confMgr = new ConfigManager(in_configPath, constraintPath, argv[0]);

    LOG(INFO) << "Time Launch : " << MTime::nowTimeStamp();

    IDBClient *dbCli = nullptr;
#ifdef use_POSTGRESQL
    dbCli = new PostgreSQLClient();
#elif use_SQLITE3
    dbCli = new SQLite3Client();
#endif

    DBMainManager *mainManager = new DBMainManager(dbCli, confMgr, &s_isWorking);
    mainManager->startMonitoringConfig();
    mainManager->startProcess();

    if(mainManager)
        delete mainManager;

    if(dbCli)
        delete dbCli;

    if(confMgr)
        delete confMgr;

    if(logStartThread.joinable())
        logStartThread.join();

    if(loggerPP)
        delete loggerPP;

    std::cerr << "Закрытие по return 0.. \n";

    return 0;
}
