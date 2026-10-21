#pragma once

#include <string>

// Appends timestamped lines to ESRunwayPlugin.log next to the DLL.
namespace logfile {

std::string PluginDirectory();
void Write(const std::string& line);

}
