#include "MetarFeed.h"

#include "Log.h"

#include <windows.h>
#include <winhttp.h>

#include <chrono>
#include <ctime>
#include <sstream>

namespace {

constexpr auto kInterval = std::chrono::seconds(60);
const wchar_t* const kHost = L"metar.vatsim.net";

// GET https://metar.vatsim.net/<path>; empty string on failure.
std::string HttpGet(const std::wstring& path)
{
    std::string body;
    HINTERNET session = WinHttpOpen(L"ESRunwayPlugin",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session)
        return body;
    WinHttpSetTimeouts(session, 10000, 10000, 10000, 10000);
    HINTERNET connection = WinHttpConnect(session, kHost, INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET request = connection ? WinHttpOpenRequest(connection, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                                        WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)
                                   : nullptr;
    if (request && WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(request, nullptr))
    {
        DWORD available = 0;
        while (WinHttpQueryDataAvailable(request, &available) && available > 0)
        {
            std::string chunk(available, '\0');
            DWORD read = 0;
            if (!WinHttpReadData(request, chunk.data(), available, &read) || read == 0)
                break;
            body.append(chunk.data(), read);
        }
    }
    if (request)
        WinHttpCloseHandle(request);
    if (connection)
        WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return body;
}

}

MetarFeed::~MetarFeed()
{
    Stop();
}

void MetarFeed::Start(const std::vector<std::string>& stations)
{
    Stop();
    std::string query;
    for (const auto& s : stations)
        query += (query.empty() ? "" : ",") + s;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_query = query;
        m_stop = false;
        m_fetchNow = true;
    }
    m_thread = std::thread(&MetarFeed::Run, this);
}

void MetarFeed::Stop()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stop = true;
    }
    m_wake.notify_all();
    if (m_thread.joinable())
        m_thread.join();
}

void MetarFeed::FetchNow()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_fetchNow = true;
    }
    m_wake.notify_all();
}

std::vector<std::string> MetarFeed::TakeLines()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> out;
    out.swap(m_lines);
    return out;
}

void MetarFeed::Run()
{
    for (;;)
    {
        std::string query;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (!m_fetchNow)
                m_wake.wait_for(lock, kInterval, [this] { return m_stop || m_fetchNow; });
            if (m_stop)
                return;
            m_fetchNow = false;
            query = m_query;
        }

        // A per-minute parameter keeps the CDN from serving a copy older than a minute.
        const std::string path = "/metar.php?id=" + query + "&m=" + std::to_string(std::time(nullptr) / 60);
        const std::string body = HttpGet(std::wstring(path.begin(), path.end()));
        if (body.empty())
        {
            logfile::Write("METAR download failed");
            continue;
        }
        std::vector<std::string> lines;
        std::istringstream ss(body);
        for (std::string line; std::getline(ss, line);)
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (!line.empty())
                lines.push_back(line);
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lines.insert(m_lines.end(), lines.begin(), lines.end());
    }
}
