#include "business/StatsManager.hpp"

namespace business
{
    StatsManager &StatsManager::getInstance()
    {
        static StatsManager instance;
        return instance;
    }

    void StatsManager::incrementRequests()
    {
        total_requests_++;
    }

    uint64_t StatsManager::getTotalRequests() const
    {
        return total_requests_.load();
    }

    void StatsManager::incrementLogMessages()
    {
        total_log_messages_++;
    }

    uint64_t StatsManager::getTotalLogMessages() const
    {
        return total_log_messages_.load();
    }

    void StatsManager::incrementErrors()
    {
        total_errors_++;
    }

    uint64_t StatsManager::getTotalErrors() const
    {
        return total_errors_.load();
    }

}