#pragma once
#include <atomic>
#include <cstdint>

namespace business
{
    class StatsManager
    {
    public:
        static StatsManager &getInstance();

        StatsManager(const StatsManager &) = delete;
        StatsManager &operator=(const StatsManager &) = delete;

        void incrementRequests();
        void incrementLogMessages();
        void incrementErrors();

        uint64_t getTotalRequests() const;
        uint64_t getTotalLogMessages() const;
        uint64_t getTotalErrors() const;

    private:
        StatsManager() = default;
        ~StatsManager() = default;

        std::atomic<uint64_t> total_requests_{0};
        std::atomic<uint64_t> total_log_messages_{0};
        std::atomic<uint64_t> total_errors_{0};
    };
}