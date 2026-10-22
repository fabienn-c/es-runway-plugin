#pragma once

#include <string>
#include <vector>

// A message box drawn on the radar screen.
struct Popup
{
    enum class Kind
    {
        Summary,        // startup result: OK
        Suggestion,     // runway change: Accept / Decline
        Info,           // "please open the runway dialog": Dismiss
        StartupPrompt   // "press OK, then open the runway dialog": OK only
    };

    int id = 0;
    Kind kind = Kind::Summary;
    std::string title;
    std::vector<std::string> lines;
    std::string icao;           // Suggestion: airport
    std::string configuration;  // Suggestion: configuration to apply
};
