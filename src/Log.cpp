#include "Log.h"

#include <windows.h>

#include <cstdio>

namespace logfile {

std::string PluginDirectory()
{
    HMODULE module = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(&PluginDirectory), &module);
    char path[MAX_PATH] = {};
    GetModuleFileNameA(module, path, MAX_PATH);
    std::string dir(path);
    const auto slash = dir.find_last_of("\\/");
    return slash == std::string::npos ? std::string(".") : dir.substr(0, slash);
}

static const std::string& LogPath()
{
    static const std::string path = PluginDirectory() + "\\ESRunwayPlugin.log";
    return path;
}

void Reset()
{
    std::remove(LogPath().c_str());
}

void Write(const std::string& line)
{
    FILE* f = std::fopen(LogPath().c_str(), "a");
    if (!f)
        return;
    SYSTEMTIME t;
    GetSystemTime(&t);
    std::fprintf(f, "%04d-%02d-%02d %02d:%02d:%02dZ  %s\n", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute,
                 t.wSecond, line.c_str());
    std::fclose(f);
}

}
