#pragma once

#include "RunwayDialog.h"

#include <windows.h>

#include <functional>
#include <set>
#include <string>
#include <vector>

// Applies runway configurations through EuroScope's runway dialog: finds the runway icon,
// opens the dialog, ticks only the runway DEP/ARR boxes of the target airports and presses OK.
// Driven by Tick() every 100 ms, so several airports take a second or two.
struct AirportTarget
{
    std::string icao;
    std::set<std::string> arrivals;
    std::set<std::string> departures;
};

class RunwayApplier
{
public:
    // ok = all targets set; message explains failures.
    using DoneCallback = std::function<void(bool ok, const std::string& message)>;
    // Called when the icon can't be found/clicked (true) and when the controller opened the dialog (false).
    using NeedUserCallback = std::function<void(bool needUser)>;

    void SetNeedUserCallback(NeedUserCallback callback) { m_needUser = std::move(callback); }

    // Returns false if already busy.
    bool Start(std::vector<AirportTarget> targets, DoneCallback done);
    bool Busy() const { return m_state != State::Idle; }
    void Tick();

private:
    enum class State
    {
        Idle,
        Opening,
        WaitingForUser,
        Ticking,
        Closing,
        Reopening  // dialog cancelled after ignored clicks, opening it again
    };

    void Finish(bool ok, const std::string& message);
    void TryOpen();
    void StepTicking(HWND dialog);

    State m_state = State::Idle;
    std::vector<AirportTarget> m_targets;
    DoneCallback m_done;
    NeedUserCallback m_needUser;
    POINT m_savedCursor = {};
    int m_ticks = 0;          // ticks spent in the current state
    int m_method = 0;         // rwydialog::ClickMethod index
    int m_openAttempts = 0;
    int m_clicks = 0;
    int m_failedClicks = 0;   // consecutive clicks without effect with the current method
    bool m_settled = false;   // the dialog has been open long enough to accept clicks
    bool m_retried = false;   // the whole pass was already repeated once
    bool m_waitingForClick = false;
    int m_clickRow = 0;
    int m_clickColumn = 0;
    std::string m_cellBefore;
    std::vector<std::string> m_missing;  // target airports absent from the dialog
};
