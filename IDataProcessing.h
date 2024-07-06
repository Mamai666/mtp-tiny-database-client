#ifndef IDATAPROCESSING_H
#define IDATAPROCESSING_H

#include <algorithm>
#include <list>
#include <string>
#include <json/TBaseJsonWork.h>

class dataproc_error: public std::exception
{
public:
    dataproc_error(const std::string& error): errorMsg{error}
    {}
    const char* what() const noexcept override
    {
        return errorMsg.c_str();
    }

private:
    std::string errorMsg = "none"; // сообщение об ошибке
};

class IDataProcessing
{
public:
    IDataProcessing(){}
    virtual ~IDataProcessing(){}

    bool hasRequest(std::string request)
    {
        bool retOK = false;
        for(auto it : requestsList())
        {
            if(it == request) {
                retOK = true;
                break;
            }
        }
        return retOK;
    }

    virtual JSON doWorkByRequest(JSON &jsonPack, const std::string &urlSender,
                                 const std::string& nameReqFunc) = 0;

protected:
    virtual std::vector<std::string> requestsList()
    {
        std::cerr << "Затычка..\n";
        return std::vector<std::string>{};
    }
};

#endif // IDATAPROCESSING_H
