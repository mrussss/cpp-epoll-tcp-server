#include <string>
#include <ctime>
#include <iomanip>
#include <sstream>
#include "business/Handlers.hpp"
#include "protocol/MessageType.hpp"
#include "business/StatsManager.hpp"
#include "business/LogStorage.hpp"

namespace business
{

    Response handlePing(const Request &request)
    {
        Response resp;
        resp.fd = request.fd;
        resp.version = request.version;
        resp.request_id = request.request_id;
        resp.type = MessageType::PONG;
        resp.payload = R"({"status":0,"message":"pong"})";
        return resp;
    }
    Response handleEcho(const Request &request)
    {
        Response resp;
        resp.fd = request.fd;
        resp.version = request.version;
        resp.request_id = request.request_id;
        resp.type = MessageType::ECHO_RESP;
        resp.payload = request.payload;
        return resp;
    }

    Response handleLogPush(const Request &request)
    {
        Response resp;
        resp.fd = request.fd;
        resp.version = request.version;
        resp.request_id = request.request_id;

        std::time_t now = std::time(nullptr);
        struct tm time_info;
        localtime_r(&now, &time_info);
        std::ostringstream oss;
        oss << std::put_time(&time_info, "[%Y-%m-%d %H:%M:%S]")
            << " fd=" << request.fd
            << " request_id=" << request.request_id
            << " payload=" << request.payload;

        StatsManager::getInstance().incrementRequests();
        bool is_written = LogStorage::getInstance().append(oss.str());
        if (is_written)
        {
            StatsManager::getInstance().incrementLogMessages();
            resp.type = MessageType::LOG_ACK;
            resp.payload = R"({"status":"success"})";
            return resp;
        }
        else
        {
            StatsManager::getInstance().incrementErrors();
            resp.type = MessageType::ERROR_RESP;
            resp.payload = R"({"status":500, "message":"log write failed."})";
            return resp;
        }
    }
    Response handleStats(const Request &request)
    {
        Response resp;
        resp.fd = request.fd;
        resp.version = request.version;
        resp.request_id = request.request_id;
        resp.type = MessageType::STATA_RESP;
        uint64_t requests = StatsManager::getInstance().getTotalRequests();
        uint64_t logMessages = StatsManager::getInstance().getTotalLogMessages();
        uint64_t errors = StatsManager::getInstance().getTotalErrors();
        std::string json = "\"total_requests\": ";
        json += std::to_string(requests);
        json += ", \"total_logs\": ";
        json += std::to_string(logMessages);
        json += ", \"total_errors\": ";
        json += std::to_string(errors);
        resp.payload = json;
        return resp;
    }
    Response makeErrorResponse(const Request &request)
    {
        Response resp;
        resp.fd = request.fd;
        resp.version = request.version;
        resp.request_id = request.request_id;
        resp.type = MessageType::ERROR_RESP;
        resp.payload = R"({"status":400,"message":"unknown type"})";
        return resp;
    }

}