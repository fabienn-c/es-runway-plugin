#pragma once

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Downloads METARs for a list of stations from metar.vatsim.net in a background thread,
// once a minute, so new reports are picked up within a minute of VATSIM publishing them.
class MetarFeed
{
public:
    ~MetarFeed();

    void Start(const std::vector<std::string>& stations);
    void Stop();
    void FetchNow();

    // METAR lines received since the last call (UI thread).
    std::vector<std::string> TakeLines();

private:
    void Run();

    std::thread m_thread;
    std::mutex m_mutex;
    std::condition_variable m_wake;
    bool m_stop = false;
    bool m_fetchNow = false;
    std::string m_query;
    std::vector<std::string> m_lines;
};
