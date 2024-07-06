#include "SOCIDBClient.h"
#include "Timer.h"
#include "loggerpp/include/LoggerPP.h"
#include "Utils/mstrings.h"
#include <regex>
#include <iomanip>

SOCIDBClient::SOCIDBClient(const SqlDBMS dbmsEnum) : IDBClient()
{
    m_sqlDBMS = SqlDBMSFromEnum(dbmsEnum);
}

bool SOCIDBClient::disconnect()
{
    try
    {
        m_sqlSession.close();
        LOG(DEBUG) << "Успешное закрытие SQL-соединения!";
        return true;
    }
    catch (soci::soci_error const& e)
    {
        LOG(ERROR) << "Ошибка при закрытии SQL-соединения: " << e.what();
    }
    return false;
}

bool SOCIDBClient::_connect(std::string connectionString)
{
    m_lastConnectionStr = connectionString;
    try
    {
        m_sqlSession.open(m_sqlDBMS, connectionString);
        LOG(INFO) << "Успешно соединено с \"" << connectionString << "\", "
                  << "используя \"" << m_sqlSession.get_backend_name() << "\"";

        return true;
    }
    catch (soci::soci_error const& e)
    {
        LOG(ERROR) << "Ошибка соединения \"" << connectionString << "\" : "
                   << e.what() << "\n";
    }
    catch (std::runtime_error const& e)
    {
        LOG(ERROR) << "Unexpected standard exception occurred: "
                   << e.what() << "\n";
    }
    catch (...)
    {
        LOG(ERROR) << "Unexpected unknown exception occurred.\n";
    }

    return false;
}

bool SOCIDBClient::checkConnectToDB()
{
    if(!m_sqlSession.is_connected()) {
        LOG(WARNING) << "База данных не открыта!";

        m_sqlSession.close();
        LOG(INFO) << "Попытка подключения.. (таймаут "+std::to_string(m_lastDbSets.conTimeout)+" сек)";
        if(!_connect(m_lastConnectionStr)) {
            //LOG(ERROR) << "Ошибка соединения с БД!";
            return false;
        }
    }
    return true;
}

std::string SOCIDBClient::filterToStr(std::map<std::string, std::string> filters,
                                      LIMIT_SET limSet)
{
    if(filters.empty())
        return "";

    std::string filterSubstr = "";
    std::string nonOrdinaryStr = "";
    bool isEmptyWhere = true;

    for(auto itMap = filters.begin(); itMap != filters.end(); ++itMap)
    {
        if(itMap->first.find("ORDER BY") != std::string::npos) {
            nonOrdinaryStr = itMap->first+" " + nonOrdinaryStr;
            continue;
        }
        else if(itMap->first.find("LIMIT") != std::string::npos) {
            nonOrdinaryStr += " "+itMap->first+" ";
            continue;
        }

        filterSubstr += (itMap->first);

        if(itMap->second != "") {
            filterSubstr += "='" + itMap->second + "'";
        }

        isEmptyWhere = false;

        filterSubstr += " AND ";
    }

    if(!isEmptyWhere) {
        filterSubstr = " WHERE (" + filterSubstr;
        if(filterSubstr.find("AND") != std::string::npos)
        {
            Strings::chop(filterSubstr,5);
        }
        filterSubstr += ") ";
    }

    if(!nonOrdinaryStr.empty()) {
        filterSubstr += " "+nonOrdinaryStr;
    }

    if(filterSubstr.find("LIMIT") == std::string::npos
        && limSet != LIMIT_SET::OFF) {
        filterSubstr += " LIMIT " + std::to_string(c_MAXLIMIT);
    }

    LOG(DEBUG) << "filterSubstr : " << filterSubstr << "\n";

    return filterSubstr;
}

JSON SOCIDBClient::getAsJsonValue(const soci::row &row , size_t idx)
{
    soci::column_properties props = row.get_properties(idx);
    LOG(DEBUG) << "props.get_data_type(): " << props.get_data_type();// == soci::data_type
    int dtp = props.get_data_type();
    JSON resValue;
    if(row.get_indicator(idx) == soci::i_null)
    {
        LOG(WARNING) << "get_indicator("<<props.get_name()<<") == soci::i_null";
    }

    try
    {
        switch (dtp) {
        case soci::data_type::dt_string:
            resValue = row.get<std::string>(idx, "");
            break;
        case soci::data_type::dt_date: {
            std::tm dateValue;
            dateValue = row.get<std::tm>(idx);

            char buffer[80];
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &dateValue);

            resValue = std::string(buffer);;
            break;
        }
        case soci::data_type::dt_double:
            resValue = row.get<double>(idx);
            break;
        case soci::data_type::dt_integer:
            resValue = row.get<int32_t>(idx);
            break;
        case soci::data_type::dt_long_long:
            resValue = row.get<int64_t>(idx);
            break;
        case soci::data_type::dt_unsigned_long_long:
            resValue = row.get<uint64_t>(idx);
            break;

        default:
            LOG(ERROR) << "Неизвестный тип колонки "<<props.get_name()<<":" <<dtp;
        }
    }
    catch (const soci::soci_error& e) {
        LOG(ERROR) << "Ошибка получения значения колонки "<<props.get_name()<<": " << e.what();
    }
    return resValue;
}

IDBClient::ErrStatus SOCIDBClient::getFilterRow(std::vector<JSON> &resultVarMap,
                                             std::map<std::string, std::string> filters,
                                             std::string needColumns)
{
    if(!checkConnectToDB()) {
        return ErrStatus::ErrConnectDB;
    }

    if(Strings::endsWith(needColumns, ",")) {
        Strings::chop(needColumns,1);
    }

    std::string queryStr = "SELECT "+needColumns+" FROM "+ getTableName() + filterToStr(filters) ;
    LOG(DEBUG) << queryStr;

    //m_sqlSession.reconnect()
    try
    {
        // Создаем объект rowset для хранения результатов запроса
        soci::rowset<soci::row> rows = (m_sqlSession.prepare << queryStr);

        LOG(DEBUG) << "Get full row is ok! " ;

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

                if(Strings::toLower(columnName) == "timestamp_cast")
                    columnName = "timestamp";

                oneResultMap[columnName] = getAsJsonValue(row, i);
                LOG(DEBUG) << "Column " << columnName << ": " << oneResultMap[columnName];
            }

            if(!oneResultMap.empty()) {
                //LOG(DEBUG) << "resultVarMap.push_back(oneResultMap)";
                resultVarMap.push_back(oneResultMap);
            }
        }

        if(resultVarMap.empty()) {
            LOG(DEBUG) << "resultVarMap пуст :(";
            return ErrStatus::NotExists;
        }
    }
    catch (const soci::soci_error& e) {
        LOG(WARNING) << "Ошибка запроса " << queryStr;
        LOG(ERROR) << "SOCI Error: " << e.what() << std::endl;

        if(std::string(e.what()).find("relation "+getTableName()+" does not exist") != std::string::npos)
        {
            return ErrStatus::NotCreateTable;
        }

        return ErrStatus::ErrSendQuery;
    }

    return ErrStatus::NoError;
}

std::string SOCIDBClient::sqlDBMS() const
{
    return m_sqlDBMS;
}

std::vector<std::string> SOCIDBClient::getTableNames()
{
    std::vector<std::string> tableNames;
    tableNames.resize(3000);
    try {
        // Use database-specific queries to retrieve table names
        if (m_sqlSession.get_backend_name() == "postgresql") {
            // PostgreSQL query
            m_sqlSession << "SELECT table_name FROM information_schema.tables WHERE table_schema = 'public'", soci::into(tableNames);
        }
        else if (m_sqlSession.get_backend_name() == "sqlite3") {
            // SQLite3 query
            m_sqlSession << "SELECT name FROM sqlite_master WHERE type='table'", soci::into(tableNames);
        }
        else {
            LOG(ERROR) << "Unsupported database backend: " << m_sqlSession.get_backend_name() << std::endl;
        }
    }
    catch (const soci::soci_error& e) {
        LOG(ERROR) << "SOCI Error: " << e.what() << std::endl;
    }

    return tableNames;
}

bool positionLessThan(const columnTableDb_t &c1, const columnTableDb_t &c2)
{
    return c1.position < c2.position;
}

IDBClient::ErrStatus SOCIDBClient::createTable(const std::string newTableName, std::vector<columnTableDb_t> columns)
{
    if(!checkConnectToDB()) {
        return ErrStatus::ErrConnectDB;
    }

    Timer tmr; tmr.start();
    std::vector<std::string> tablesHave = getTableNames();
    LOG(DEBUG) << "Timer tablesHave elaps: " << tmr.elapsedMicroseconds();
    LOG(DEBUG) << "Tables: " << tablesHave;

    std::string newTableNameNoQuotes = newTableName;
    //newTableNameNoQuotes.replace("\"", "");
    newTableNameNoQuotes = std::regex_replace(newTableNameNoQuotes, std::regex("\""), "");
    for(auto& it : tablesHave)
    {
        if(Strings::toLower(it) == Strings::toLower(newTableNameNoQuotes)) {
            LOG(WARNING) << newTableNameNoQuotes << " already created !!!";
            return ErrStatus::AlreadyExists;
        }
    }

    std::string columnsStr = "";
    // Сортировка колонок по возрастанию position
    std::stable_sort(columns.begin(),columns.end(), positionLessThan);

    for(const auto &itColumn : columns) {
        columnsStr.append(itColumn.name +" "+ itColumn.dataType + ",");
    }

    std::string indexKeyStr = "";
    if(newTableName == "info_nodes" || newTableName == "\"info_nodes\"")
    {
        //pid INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT

        indexKeyStr = " pid int8 NOT NULL GENERATED ALWAYS AS IDENTITY,\
            idx int8 NOT NULL GENERATED ALWAYS AS IDENTITY,\
            CONSTRAINT info_nodes_pk PRIMARY KEY (pid),";
    }
    else
    {
        if (m_sqlSession.get_backend_name() == "postgresql") {
            indexKeyStr = "idx int8 NOT NULL GENERATED ALWAYS AS IDENTITY,";
        }
        else if (m_sqlSession.get_backend_name() == "sqlite3") {
            indexKeyStr = "idx INTEGER PRIMARY KEY AUTOINCREMENT,";
        }
    }

    if(!columnsStr.empty()) {
        Strings::chop(columnsStr, 1); // Удаление запятой в конце
    }
    else {
        Strings::chop(indexKeyStr, 1); // Удаление запятой в конце
    }

    std::string queryStr = "CREATE TABLE " + newTableName + " ( "+indexKeyStr+" "+columnsStr+" )";
    queryStr = Strings::toLower(queryStr);
    LOG(DEBUG) << queryStr;
    try
    {
        m_sqlSession << queryStr;
        LOG(DEBUG) << "CREATE TABLE success!";
    }
    catch (const soci::soci_error& e) {
        LOG(WARNING) << "Ошибка запроса " << queryStr;
        LOG(ERROR) << "SOCI Error: " << e.what() << std::endl;
        return ErrStatus::ErrSendQuery;
    }

    return ErrStatus::NoError;
}

IDBClient::ErrStatus SOCIDBClient::renameTable(std::string oldTableName, std::string newTableName)
{
    LOG(ERROR) << "Ошибка! Вызов нереализованной функции! ";
    return ErrStatus::UndefinedError;
}

IDBClient::ErrStatus SOCIDBClient::removeTable(std::string tableName)
{
    LOG(ERROR) << "Ошибка! Вызов нереализованной функции! ";
    return ErrStatus::UndefinedError;
}

IDBClient::ErrStatus SOCIDBClient::updateColumn(std::string nameColumn, std::string value, std::string uniqKey, std::string uniqValue)
{
    if(!checkConnectToDB()) {
        return ErrStatus::ErrConnectDB;
    }

    uniqValue = std::regex_replace(uniqValue, std::regex("'"), "");
    value = std::regex_replace(value, std::regex("'"), "");

    std::string queryStr = "SELECT 1 FROM "+ m_tableName +" WHERE "+ uniqKey +"='" + uniqValue + "'";
    LOG(DEBUG) << queryStr;
    try
    {
        // Создаем объект rowset для хранения результатов запроса
        soci::rowset<soci::row> rows = (m_sqlSession.prepare << queryStr);
        if(rows.begin() == rows.end())
        {
            LOG(ERROR) << "Field" << uniqKey << "with value" << uniqValue << "is not exists!";
            return ErrStatus::ErrSendQuery;
        }

        queryStr = "UPDATE "+ m_tableName +" SET " + nameColumn + "='" + value +
                   "' WHERE "+ uniqKey +"='" + uniqValue + "'";

        LOG(DEBUG) << queryStr;
        m_sqlSession << queryStr;
    }
    catch (const soci::soci_error& e) {
        LOG(WARNING) << "Ошибка запроса " << queryStr;
        LOG(ERROR) << "SOCI Error: " << e.what() << std::endl;
        return ErrStatus::ErrSendQuery;
    }
    return ErrStatus::NoError;
}

IDBClient::ErrStatus SOCIDBClient::insertRow(std::vector<columnTableDb_t> columns, std::string uniqKey, std::string uniqValue)
{
    if(!checkConnectToDB()) {
        return ErrStatus::ErrConnectDB;
    }

    std::string rowNamesStr, rowValuesStr ;
    for(const auto &itColumn : columns)
    {
        rowNamesStr.append(itColumn.name + ",");
        rowValuesStr.append(itColumn.value + ",");
    }

    if(!rowNamesStr.empty()) {
        Strings::chop(rowNamesStr,1); // Удаление запятой в конце
    }
    if(!rowValuesStr.empty()) {
        Strings::chop(rowValuesStr,1); // Удаление запятой в конце
    }

    if(uniqKey == "" && uniqValue == "")
    {
        std::string queryStr = "INSERT INTO "+ m_tableName +" ("+rowNamesStr+") VALUES ("+rowValuesStr+")";
        LOG(DEBUG) << queryStr;
        try
        {
            m_sqlSession << queryStr;
            return ErrStatus::NoError;
        }
        catch (const soci::soci_error& e) {
            LOG(WARNING) << "Ошибка запроса " << queryStr;
            LOG(ERROR) << "SOCI Error: " << e.what() << std::endl;
            return ErrStatus::ErrSendQuery;
        }
    }

    uniqValue = std::regex_replace(uniqValue, std::regex("'"), "");

    std::string queryStr = "SELECT 1 FROM "+ m_tableName +" WHERE "+ uniqKey +"='" + uniqValue + "'";
    //  queryStr = queryStr.toLower();
    LOG(DEBUG) << queryStr;
    try
    {
        // Создаем объект rowset для хранения результатов запроса
        soci::rowset<soci::row> rows = (m_sqlSession.prepare << queryStr);
        if(rows.begin() == rows.end())
        {
            LOG(WARNING) << "Field" << uniqKey << "with value" << uniqValue << "is not exists!";
            queryStr = "INSERT INTO "+ m_tableName +" (" + rowNamesStr + ") VALUES ("+rowValuesStr+")";
            m_sqlSession << queryStr;
        }
        else
        {
            std::vector<std::string> listNames  = Strings::stdv_split(rowNamesStr, ",");
            std::vector<std::string> listValues = Strings::stdv_split(rowValuesStr, ",");

            queryStr = "UPDATE "+ m_tableName +" SET ";

            for(const auto &itColumn : columns)
            {
                queryStr.append(itColumn.name + "=" + itColumn.value + ",");
            }
            Strings::chop(queryStr, 1);
            queryStr.append(" WHERE "+ uniqKey +"='" + uniqValue + "'");
            //       queryStr = queryStr.toLower();
            m_sqlSession << queryStr;
        }
    }
    catch (const soci::soci_error& e) {
        LOG(WARNING) << "Ошибка запроса " << queryStr;
        LOG(ERROR) << "SOCI Error: " << e.what() << std::endl;
        return ErrStatus::ErrSendQuery;
    }
    return ErrStatus::NoError;
}

IDBClient::ErrStatus SOCIDBClient::addNewColumn(std::string nameColumn, std::string dataType)
{
    if(!checkConnectToDB()) {
        return ErrStatus::ErrConnectDB;
    }

    std::string queryStr = "ALTER TABLE "+ m_tableName +" ADD COLUMN "+ nameColumn +" "+ dataType;
    queryStr = Strings::toLower(queryStr);
    LOG(DEBUG) << queryStr;
    try
    {
        m_sqlSession << queryStr;
        return ErrStatus::NoError;
    }
    catch (const soci::soci_error& e) {
        LOG(WARNING) << "Ошибка запроса " << queryStr;
        LOG(ERROR) << "SOCI Error: " << e.what() << std::endl;
        return ErrStatus::ErrSendQuery;
    }
    return ErrStatus::NoError;
}

IDBClient::ErrStatus SOCIDBClient::deleteRow(std::string nameField, std::string valueField)
{
    if(!checkConnectToDB()) {
        return ErrStatus::ErrConnectDB;
    }

    valueField = std::regex_replace(valueField, std::regex("'"), "");
    std::string queryStr = "SELECT 1 FROM "+ m_tableName +" WHERE "+ nameField + "='" + valueField + "'";
    LOG(DEBUG) << queryStr;
    try
    {
        // Создаем объект rowset для хранения результатов запроса
        soci::rowset<soci::row> rows = (m_sqlSession.prepare << queryStr);
        if(rows.begin() == rows.end())
        {
            LOG(WARNING) << "Field" << nameField << "with value" << valueField << "is not exists!";
            return ErrStatus::ErrSendQuery;
        }

        queryStr = "DELETE FROM "+ m_tableName +" WHERE "+ nameField +"='"+ valueField +"'";
        LOG(DEBUG) << queryStr;
        m_sqlSession << queryStr;
        LOG(DEBUG) << "delete success!";
    }
    catch (const soci::soci_error& e) {
        LOG(WARNING) << "Ошибка запроса " << queryStr;
        LOG(ERROR) << "SOCI Error: " << e.what() << std::endl;
        return ErrStatus::ErrSendQuery;
    }

    return ErrStatus::NoError;
}

IDBClient::ErrStatus SOCIDBClient::listExistColumns(std::map<std::string, std::string> &existColumns)
{
    if(!checkConnectToDB()) {
        return ErrStatus::ErrConnectDB;
    }

    std::string tableName = std::regex_replace(getTableName(), std::regex("\""), "");
    std::string queryStr = "";
    // Use database-specific queries to retrieve column information
    if (m_sqlSession.get_backend_name() == "postgresql") {
        // PostgreSQL query
        queryStr = "SELECT column_name, data_type FROM information_schema.columns " \
                   "WHERE table_name = '"+tableName+"'";
    }
    else if (m_sqlSession.get_backend_name() == "sqlite3") {
        // SQLite3 query
        queryStr = "PRAGMA table_info(" + tableName + ")";
    }
    else {
        LOG(ERROR) << "Unsupported database backend: " << m_sqlSession.get_backend_name() << std::endl;
        return ErrStatus::ErrSendQuery;
    }

    //qDebug().noquote() << queryStr;
    queryStr = Strings::toLower(queryStr);
    LOG(DEBUG) << queryStr;

    try
    {
        // Создаем объект rowset для хранения результатов запроса
        soci::rowset<soci::row> rows = (m_sqlSession.prepare << queryStr);
        if (m_sqlSession.get_backend_name() == "postgresql") {
            for (const auto& row : rows) {
                auto colName = row.get<std::string>("column_name");
                auto dataType = row.get<std::string>("data_type");
                existColumns[colName] = dataType;
                //LOG(DEBUG) << "existColumns["<<colName<<"]: " << existColumns[colName];
            }
        }
        else if (m_sqlSession.get_backend_name() == "sqlite3") {
            for (const auto& row : rows) {
                auto colName = row.get<std::string>("name");
                auto dataType = row.get<std::string>("type");
                existColumns[colName] = dataType;
            }
        }

        if(existColumns.empty())
        {
            LOG(DEBUG) << "existColumns пуст :(";
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

std::string SOCIDBClient::getTableName() const
{
    return m_tableName;
}

void SOCIDBClient::setTableName(std::string value, bool needQuotes)
{
    if(needQuotes)
    {
        LOG(DEBUG) << "Table Before: " << (value.empty() ? "[EMPTY]" : value);
        if(!value.empty() && value.at(0) != '"') {
            value = '"' + value + '"';
        }
        LOG(DEBUG) << "Table After: " << (value.empty() ? "[EMPTY]" : value);
    }
    m_tableName = value.empty() ? "" : Strings::toLower(value);
}


