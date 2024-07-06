#include "PostgreSQLClient.h"
#include "loggerpp/include/LoggerPP.h"

PostgreSQLClient::PostgreSQLClient() : SOCIDBClient(SqlDBMS::PostgreSQL)
{

}

PostgreSQLClient::~PostgreSQLClient()
{

}

bool PostgreSQLClient::connect(DBSettings &dbSets)
{
    m_lastDbSets = dbSets;
    std::string conStr = "dbname="+dbSets.nameDB+
                         " host="+dbSets.host+
                         " port="+std::to_string(dbSets.port)+
                         " user="+dbSets.userName+
                         " password="+dbSets.password+
                         " connect_timeout="+std::to_string(dbSets.conTimeout)+
                         " singlerows=false";

    bool retOK = _connect(conStr);

    return retOK;
}

IDBClient::ErrStatus PostgreSQLClient::createFunction(std::string text)
{
    if(!checkConnectToDB()) {
        return ErrStatus::ErrConnectDB;
    }

    std::string queryStr = text;

    LOG(DEBUG) << queryStr;
    try
    {
        m_sqlSession << queryStr;
        LOG(DEBUG) << "CREATE FUNCTION success!";
    }
    catch (const soci::soci_error& e) {
        LOG(WARNING) << "Ошибка запроса " << queryStr;
        LOG(ERROR) << "SOCI Error: " << e.what() << std::endl;
        return ErrStatus::ErrSendQuery;
    }

    return ErrStatus::NoError;
}


IDBClient::ErrStatus PostgreSQLClient::getTimeLine(std::vector<JSON> &resultVarMap,
                                                   std::string &timeBegin, std::string &timeEnd, std::string &camSource)
{
    if(!checkConnectToDB()) {
        return ErrStatus::ErrConnectDB;
    }

    std::string filterWhereRow = "(\"timestamp-begin\" between beginT and endT) and \"source\"=camSource";
    std::string inputFuncArgs = "beginT timestamp, endT timestamp, camSource text";
    // ********* PREPARE SQL FUNCTION **************
    std::string funcText = "CREATE OR REPLACE FUNCTION continue_time_frame("+inputFuncArgs+") \
                        RETURNS table(\"timestamp-begin-sub\" timestamp, \"timestamp-end-sub\" timestamp, grp bigint) \
    AS \
    $$ \
        SELECT \"timestamp-begin\", \"timestamp-end\", count(*) FILTER (WHERE step) \
        OVER (ORDER BY \"timestamp-begin\") AS grp \
        FROM  ( SELECT \"timestamp-begin\", \"timestamp-end\", \
            (lag(\"timestamp-begin\") OVER (ORDER BY \"timestamp-begin\") <= \"timestamp-begin\" - interval '1.5 min') \
            AS step FROM archivewriter where "+filterWhereRow+" ) sub ORDER  BY \"timestamp-begin\" \
    $$ language sql; ";

    auto retErr = createFunction(funcText);
    if(retErr != IDBClient::NoError) {
        LOG(ERROR) << "ERROR Create FUNCTION continue_time_frame!";
        return ErrStatus::ErrSendQuery;
    }

    std::string inputArgRow = "'"+timeBegin+"','"+timeEnd+"','"+camSource+"'";
    std::string queryStr = "SELECT DISTINCT FIRST_VALUE(\"timestamp-begin-sub\") \
                        OVER(PARTITION BY grp ORDER BY \"timestamp-begin-sub\") AS \"begin-sub\",  \
                        FIRST_VALUE(\"timestamp-end-sub\") OVER(PARTITION BY grp ORDER BY \"timestamp-end-sub\" desc) \
                        AS \"end-sub\", grp \
                        FROM continue_time_frame("+inputArgRow+") LIMIT " +std::to_string(MAX_LIMIT)+ ";";

    LOG(DEBUG) << queryStr;
    try
    {
        // Создаем объект rowset для хранения результатов запроса
        soci::rowset<soci::row> rows = (m_sqlSession.prepare << queryStr);
        LOG(DEBUG) << "Get row from continue_time_frame is ok! " ;

        for (const auto& row : rows) {
            // Получаем количество столбцов в текущей строке
            std::size_t numColumns = row.size();
            LOG(DEBUG) << "Получено колонок: " << numColumns;

            JSON oneResultMap;
            // Обрабатываем каждый столбец
            for (std::size_t i = 0; i < numColumns; ++i) {
                // Используем get без указания типа для динамического получения значения
                soci::column_properties props = row.get_properties(i);
                std::string columnName = props.get_name();
                LOG(DEBUG) << "Column " << columnName << " before oneResultMap[columnName] =";

                // if(Strings::toLower(columnName) == "timestamp_cast")
                //     columnName = "timestamp";

                oneResultMap[columnName] = getAsJsonValue(row, i);
                LOG(DEBUG) << "Column " << columnName << ": " << oneResultMap[columnName];
            }

            if(!oneResultMap.empty())
                resultVarMap.push_back(oneResultMap);
        }

        if(resultVarMap.empty()) {
            LOG(DEBUG) << "resultVarMap пуст :(";
            return ErrStatus::NotExists;
        }
    }
    catch (const soci::soci_error& e) {
        LOG(WARNING) << "Ошибка запроса " << queryStr;
        LOG(ERROR) << "SOCI Error: " << e.what() << std::endl;
        return ErrStatus::ErrSendQuery;
    }

    return ErrStatus::NoError;
}

std::string PostgreSQLClient::nameTypeForTimestamp()
{
    return "timestamp";
}

std::string PostgreSQLClient::nameTypeForText()
{
    return "text";
}

std::string PostgreSQLClient::nameTypeForJson()
{
    return "jsonb";
}

std::string PostgreSQLClient::nameTypeForBool()
{
    return "int4";
}

std::string PostgreSQLClient::nameTypeForInt()
{
    return "int4";
}

std::string PostgreSQLClient::nameTypeForDouble()
{
    return "real";
}
