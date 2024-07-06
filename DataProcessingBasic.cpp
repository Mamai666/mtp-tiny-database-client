#include "DataProcessingBasic.h"
#include "Utils/mstrings.h"
#include "loggerpp/include/LoggerPP.h"

#include "Timer.h"
#include <regex>

DataProcessingBasic::DataProcessingBasic(IDBClient* pdbCli) : IDataProcessing()
{
    p_db = pdbCli;
}

JSON DataProcessingBasic::doWorkByRequest(JSON &jsonPack, const std::string &urlSender,
                                          const std::string& nameReqFunc)
{
    answerOnRequest_t answerPack;
    JSON outJson;
    outJson["status"] = false;

    auto retErr = IDBClient::ErrSendQuery;
    std::string lastUsedTable = p_db->getTableName();

    if(nameReqFunc == "update")
    {
        if(jsonPack.contains("unique-name"))
        {
            std::string uniqName  = jsonPack.value("unique-name", "");
            std::string uniqValue = jsonPack.value("unique-value", "");

            LOG(INFO) << "For row with " << uniqName << " == " << uniqValue;
            retErr = updateProc(jsonPack, uniqName, uniqValue);
        }
        else
        {
            answerPack.status = false;
            answerPack.error = "Json not contains unique-name!";
        }
    }
    else if(nameReqFunc == "delete" || nameReqFunc == "deleteLayout")
    {
        if(jsonPack.contains("unique-name"))
        {
            std::string uniqName  = jsonPack.value("unique-name", "");
            std::string uniqValue = jsonPack.value("unique-value", "");

            LOG(INFO) << "Delete row with " << uniqName << " == " << uniqValue;
            retErr = deleteProc(jsonPack, uniqName, uniqValue);
        }
        else
        {
            answerPack.status = false;
            answerPack.error = "Json not contains unique-name!";
        }
    }
    else if(nameReqFunc == "addNewRecord")
    {
        answerPack.status = addNewRecord(jsonPack, false, true);
    }
    else if(nameReqFunc == "getAll")
    {
        std::string uniqName  = jsonPack.value("unique-name", "");
        std::string uniqValue = jsonPack.value("unique-value", "");
        retErr = getAll(jsonPack, uniqName, uniqValue, answerPack);
    }
    else {
        outJson["error"] = "Не найден требуемый запрос в if ("+nameReqFunc+") ";
        return outJson;
    }

    if(retErr != IDBClient::ErrSendQuery &&
        retErr != IDBClient::ErrConnectDB &&
        retErr != IDBClient::UndefinedError)
    {
        answerPack.status = true;
    }
    else if(!answerPack.status && answerPack.error.empty()) {
        answerPack.error = "Any ErrStatus";//metaEnum.valueToKey(retErr);
    }

    if(!lastUsedTable.empty()) {
        p_db->setTableName(lastUsedTable);
    }

    // ----------- Нужно постепенно отказаться от answerPack и писать напрямую в JSON --
    convertAnswerPackToJson(outJson, nameReqFunc, answerPack);
    // ---------------------------------------------------------

    return outJson;
}

IDBClient::ErrStatus DataProcessingBasic::getAll(JSON& jsonPack,
                                                 std::string searchKeyName, JSON searchKeyValue,
                                                 answerOnRequest_t &answerPack)
{
    auto retErr = IDBClient::ErrSendQuery;

    if(!searchKeyName.empty())
    {
        LOG(INFO) << "Get All info about something with " << searchKeyName << " == " << searchKeyValue;
        std::map<std::string, std::string> filters;
        filters[searchKeyName] = searchKeyValue;
        retErr =  p_db->getFilterRow(answerPack.dataMaps["data"], filters); // Get info about all rows
    }
    else
    {
        std::string prmName      = jsonPack.value("param-name", "");
        std::string prmCondition = jsonPack.value("param-condition", "");

        std::map<std::string, std::string> filters;
        if(!prmName.empty() && !prmName.empty()) {
            filters.insert({"\""+prmName+"\" " + prmCondition, ""});
        }

        std::string orderDirection = jsonPack.value("order", "desc");
        int limit = jsonPack.value("limit", 0);

        filters.insert({"ORDER BY idx " + orderDirection, ""});
        limit = (limit > MAX_LIMIT) ? MAX_LIMIT : limit;
        if(limit > 0) {
            filters.insert({"LIMIT "+std::to_string(limit), ""});
        }

        retErr = p_db->getFilterRow(answerPack.dataMaps["data"], filters);
    }

    return retErr;
}

IDBClient::ErrStatus DataProcessingBasic::deleteProc(JSON &jsonPack, std::string searchKeyName, JSON searchKeyValue)
{
    auto retErr = IDBClient::ErrSendQuery;
    retErr = p_db->deleteRow(searchKeyName, static_cast<std::string>(searchKeyValue));
    return retErr;
}

IDBClient::ErrStatus DataProcessingBasic::updateProc(JSON &jsonPack, std::string searchKeyName, /*QVariant*/ JSON searchKeyValue)
{
    auto retErr = IDBClient::ErrSendQuery;
    LOG(DEBUG) << "Data:";

    std::string addColumn = "";
    JSON::array_t format;
    if(jsonPack.contains("Format") && jsonPack["Format"].is_array()) {
        format = jsonPack.value("Format", JSON::array_t{});
    }
    jsonPack.erase("Format");

    std::vector<columnTableDb_t> columnsTable;

    for (auto it = jsonPack.begin(); it != jsonPack.end(); it++) // Подразумевается объект changes
    {
        if(it->is_object())
        {
            JSON jsObj = it.value();
            for (auto itInto = jsObj.begin(); itInto != jsObj.end(); itInto++) {
                add(itInto,format, columnsTable);
            }
        }
    }

    for(auto& itColumn : columnsTable) {
        if(searchKeyValue.is_string()) {
            retErr = p_db->updateColumn(itColumn.name, itColumn.value,
                                        searchKeyName,
                                        static_cast<std::string>(searchKeyValue));
        }
    }
    return retErr;
}

bool DataProcessingBasic::addNewRecord(JSON jsonPack, bool flagWriteAll, bool addIndexColumn)
{
    std::vector<columnTableDb_t> columnsTable;

    bool statusOK = false;

    Timer tmr; tmr.start();
    JSON::array_t format;
    if(jsonPack.contains("Format") && jsonPack["Format"].is_array()) {
        format = jsonPack.value("Format", JSON::array_t{});
    }
    jsonPack.erase("Format");

    std::string uniqName = "", uniqValue = "";

    int64_t lastElaps = 0;

    if(jsonPack.contains("unique-name") && jsonPack.contains("unique-value"))
    {
        uniqName = jsonPack.value("unique-name", "");
        uniqValue = jsonPack.value("unique-value", "");
    }

    for (auto it = jsonPack.begin(); it != jsonPack.end(); it++)
    {
        LOG(DEBUG) << "it : " << it.key() << " " << it.value();
        if(flagWriteAll) // Запись всех значений, кроме blacklist. Только одноуровневый Json!!
        {
            add(it, format, columnsTable);
            continue;
        }

        // Запись значений из секции toWrite, кроме blacklist.
        if(it.key() != "toWrite") {
            continue;
        }
        else if(it->is_object())
        {
            extractValueFromObject(it.value(), format, columnsTable, uniqName, uniqValue);

        }
        else if(it->is_array())
        {
            for(const auto &itObj : it.value())
            {
                columnsTable.clear();
                if(itObj.is_object())
                {
                    extractValueFromObject(itObj, format, columnsTable, uniqName, uniqValue);
                    statusOK = putValueInTable(columnsTable, uniqName, uniqValue);
                }
            }
            return statusOK;
        }
    }

    lastElaps =  tmr.elapsedMicroseconds();
    statusOK = putValueInTable(columnsTable, uniqName, uniqValue); // САМАЯ ЗАТРАТНАЯ ФУНКЦИЯ
    LOG(DEBUG) << "putValueInTable elaps Mks : " <<  tmr.elapsedMicroseconds() - lastElaps;

    return statusOK;
}

void DataProcessingBasic::add(JSON::iterator &it, JSON::array_t &format, std::vector<columnTableDb_t> &columnsTable)
{
    auto &v = m_blackColumnList;
    if(std::find(v.begin(), v.end(), it.key()) != v.end()) {
        return ;
    }

    if(it.value().is_string() && it.value().empty()) {
        LOG(DEBUG) << "Value of " << it.key() << " is null!! == " << it.value() ;
        //return ;
    }

    if(it.value().is_array() && it.value().empty()) {
        LOG(WARNING) << "Array " << it.key() << " is empty!! Skip this..";
        return ;
    }

    auto colTable = defineTypeAndValue(it.key(), it.value(), format);
    columnsTable.push_back(colTable); // Массив структур, содержащих одну ячейку таблицы (значение, тип данных и имя столбца)
}

bool DataProcessingBasic::putValueInTable(std::vector<columnTableDb_t> &columnsTable,
                                         const std::string &uniqName, const std::string &uniqValue)
{
    //   qDebug() << "UniqName ==" << m_uniqName+"; UniqValue ==" << m_uniqValue;
    auto retCT = p_db->createTable(p_db->getTableName(), columnsTable); // Создание таблицы, если не создана
    Timer tmrAddColumn; tmrAddColumn.start();
    if(retCT == IDBClient::AlreadyExists &&
        (p_db->getTableName().find("info_nodes") != std::string::npos ||
         p_db->getTableName().find("registeredanalytics") != std::string::npos )
      )
    {
        std::map<std::string, std::string> existColumns;
        retCT = p_db->listExistColumns(existColumns);
        if(retCT == IDBClient::NoError)
        {
            for(auto& itColumn : columnsTable)
            {
                //      elapsed 20 ms!!
                std::string colNameLower = Strings::toLower(itColumn.name);
                colNameLower = std::regex_replace(colNameLower, std::regex("\""), "");
                if(existColumns.find(colNameLower) == existColumns.end()) {
                    retCT = p_db->addNewColumn(itColumn.name, itColumn.dataType); // Динамическое добавление новых колонок
                }
            }
        }
    }
    LOG(DEBUG) << "tmrAddColumn elapse, ms: " << tmrAddColumn.elapsedMilliseconds();
    if(retCT != IDBClient::ErrConnectDB && retCT != IDBClient::UndefinedError)
    {
        // elapsed 1-2 ms
        retCT = p_db->insertRow(columnsTable, uniqName, uniqValue); // Добавление новой записи устройства
        if(retCT == IDBClient::NoError || retCT == IDBClient::AlreadyExists) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> DataProcessingBasic::requestsList()
{
    return m_cSupportRequests;
}

void DataProcessingBasic::extractValueFromObject(JSON jsObj, JSON::array_t &format, std::vector<columnTableDb_t> &columnsTable,
                                                std::string &uniqName, std::string &uniqValue)
{
    for (auto itInto = jsObj.begin(); itInto != jsObj.end(); itInto++)
    {
        add(itInto, format, columnsTable);
    }

    if(jsObj.contains("unique-name") && jsObj.contains("unique-value"))
    {
        uniqName = jsObj.value("unique-name", "");
        uniqValue = jsObj.value("unique-value", "");
    }
}

columnTableDb_t DataProcessingBasic::autoDefineType(std::string &key, JSON &value)
{
    columnTableDb_t result;

    if(value.is_string())
    {
        bool hasMatch = false;
        if(!value.empty())
        {
            std::regex dateTimeRX("^([0-9]{4,4})-([0-1]{1,1}[0-9]{1,1})-([0-3]{1,1}[0-9]{1,1}) "
                                          "([0-2]{1,1}[0-9]{1,1}):([0-5]{1,1}[0-9]{1,1}):(([0-5]{1,1}[0-9]{1,1})"
                                          "|([0-5]{1,1}[0-9]{1,1})([.]([0-9]{1,6})))$");

            hasMatch = std::regex_match(static_cast<std::string>(value), dateTimeRX); //::NormalMatch);
            //  bool hasPartialMatch = matchDateTime.hasPartialMatch();
        }

        if(hasMatch) {
            LOG(DEBUG) << "detect datatype timestamp!";
            result.dataType = p_db->nameTypeForTimestamp();
        }
        else {
            LOG(DEBUG) << "detect datatype text!";
            result.dataType = "text";
        }

        result.value = value;
    }
    else if(value.is_object())
    {
        result.dataType = p_db->nameTypeForJson();
        result.value = value.dump(); // need do compact space
    }
    else if(value.is_array())
    {
        result.dataType = p_db->nameTypeForJson();
        result.value = value.dump(); // need do compact space
    }
    else if(value.is_boolean())
    {
        result.dataType = p_db->nameTypeForBool();
        result.value = value ? 1 : 0;
    }
    else if(value.is_number_integer())
    {
        result.dataType = p_db->nameTypeForInt();
        result.value = std::to_string(static_cast<int>(value));
    }
    else if(value.is_number_float())
    {
        result.dataType = p_db->nameTypeForDouble();
        result.value = std::to_string(static_cast<float>(value));
    }
    return result;
}

columnTableDb_t DataProcessingBasic::formatDefineType(std::string &key, JSON &value, JSON::array_t &format)
{
    columnTableDb_t result;
    int position = 0;

    for(const JSON &itFormat : format)
    {
        //JSON oneFormat = itFormat;
        const std::string name = itFormat.value("Name","");
        const std::string type = itFormat.value("Type","");
        if(name == key && !name.empty())
        {
            result.dataType = type;
            result.position = position;
            if(value.is_string()) {
                result.value = value;
            }
            else if(value.is_boolean()) {
                result.value = value ? "true" : "false";
            }
            else if(value.is_number_integer()) {
                result.value = std::to_string(static_cast<int>(value));
            }
            else if(value.is_number_float()) {
                result.value = std::to_string(static_cast<float>(value));
            }
            else {
                result.dataType = "";
                result.value = "";
                result.position = -1;
            }
            break;
        }
        position++;
    }

    return result;
}

columnTableDb_t DataProcessingBasic::defineTypeAndValue(std::string key, JSON value, JSON::array_t &format)
{
    columnTableDb_t result;

    if(!format.empty()) {
        result = formatDefineType(key, value, format);
    }

    if(result.dataType.empty()) {
        auto mayConstainPosition = result.position;
        result = autoDefineType(key, value);
        result.position = mayConstainPosition;
    }

    if(!result.dataType.empty())
    {
        result.name = key;
        if(result.name.find("-") != std::string::npos) {
            result.name = '"' + result.name;
            result.name += '"';
        }

        result.value = "'" + result.value;
        result.value += "'";
    }
    return result;
}
