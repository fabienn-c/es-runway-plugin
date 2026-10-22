#include "RunwayApplier.h"

#include "Log.h"
#include "RunwayIcon.h"

namespace {

constexpr int kDialogWaitTicks = 15;       // 1.5 s for the dialog to appear after clicking the icon
constexpr int kSettleTicks = 5;            // 0.5 s after the dialog appears before the first click
constexpr int kUserWaitTicks = 1200;       // 2 min for the controller to open the dialog
constexpr int kClickWaitTicks = 6;         // 0.6 s for a tick to show
constexpr int kTriesPerMethod = 2;         // clicks without effect before trying the next method
constexpr int kCloseWaitTicks = 30;        // 3 s for the dialog to close after OK
constexpr int kReopenWaitTicks = 10;       // 1 s between closing the dialog and the retry
constexpr int kMaxClicks = 600;  // a whole sector with every runway ticked needs a few hundred
constexpr int kMethodCount = static_cast<int>(rwydialog::ClickMethod::Count);

}

bool RunwayApplier::Start(std::vector<AirportTarget> targets, DoneCallback done)
{
    if (Busy())
        return false;
    m_targets = std::move(targets);
    m_done = std::move(done);
    m_missing.clear();
    m_method = m_openAttempts = m_clicks = m_ticks = m_failedClicks = 0;
    m_waitingForClick = m_settled = m_retried = false;
    GetCursorPos(&m_savedCursor);
    m_state = State::Opening;
    std::string list;
    for (const auto& t : m_targets)
        list += " " + t.icao;
    logfile::Write("applying runways for" + list);
    return true;
}

void RunwayApplier::Finish(bool ok, const std::string& message)
{
    if (m_state == State::WaitingForUser && m_needUser)
        m_needUser(false);
    SetCursorPos(m_savedCursor.x, m_savedCursor.y);
    m_state = State::Idle;
    logfile::Write(std::string(ok ? "runways applied" : "runways NOT applied") + (message.empty() ? "" : ": " + message));
    auto done = std::move(m_done);
    m_done = nullptr;
    if (done)
        done(ok, message);
}

void RunwayApplier::Tick()
{
    if (m_state == State::Idle)
        return;
    ++m_ticks;
    const HWND dialog = rwydialog::Find();

    switch (m_state)
    {
    case State::Opening:
        if (dialog)
        {
            m_state = State::Ticking;
            m_ticks = 0;
        }
        else if (m_openAttempts == 0 || m_ticks > kDialogWaitTicks)
            TryOpen();
        break;

    case State::WaitingForUser:
        if (dialog)
        {
            if (m_needUser)
                m_needUser(false);
            m_state = State::Ticking;
            m_ticks = 0;
        }
        else if (m_ticks > kUserWaitTicks)
            Finish(false, "the runway dialog was not opened");
        break;

    case State::Ticking:
        if (!dialog)
            Finish(false, "the runway dialog was closed before the runways were set");
        else if (!m_settled && m_ticks < kSettleTicks)
            break;  // a freshly opened dialog ignores clicks for a moment
        else
        {
            m_settled = true;
            StepTicking(dialog);
        }
        break;

    case State::Reopening:
        if (!dialog && m_ticks > kReopenWaitTicks)
        {
            m_state = State::Opening;
            m_method = m_openAttempts = m_failedClicks = m_ticks = 0;
            m_waitingForClick = m_settled = false;
        }
        break;

    case State::Closing:
        if (!dialog)
        {
            std::string message;
            for (const auto& icao : m_missing)
                message += (message.empty() ? "not in the runway dialog: " : ", ") + icao;
            Finish(m_missing.empty(), message);
        }
        else if (m_ticks > kCloseWaitTicks)
            Finish(false, "the runway dialog did not close");
        break;

    default:
        break;
    }
}

void RunwayApplier::TryOpen()
{
    m_ticks = 0;
    if (m_openAttempts >= kMethodCount)
    {
        m_state = State::WaitingForUser;
        if (m_needUser)
            m_needUser(true);
        return;
    }
    // Never click blindly: only where the runway icon is actually seen.
    const auto icon = rwyicon::Locate();
    if (!icon.found)
    {
        logfile::Write("runway icon not found (best " + std::to_string(icon.mismatches) + "/" +
                       std::to_string(icon.pixels) + " pixels differ)");
        m_openAttempts = kMethodCount;
        TryOpen();
        return;
    }
    const auto method = static_cast<rwydialog::ClickMethod>(m_openAttempts++);
    logfile::Write(std::string("opening the runway dialog with ") + rwydialog::MethodName(method));
    rwydialog::ClickWindow(icon.view, icon.center, method);
}

void RunwayApplier::StepTicking(HWND dialog)
{
    const HWND list = rwydialog::List(dialog);
    const auto rows = rwydialog::ReadRows(list);

    if (m_waitingForClick)
    {
        const std::string after =
            static_cast<size_t>(m_clickRow) < rows.size() ? rows[m_clickRow].cells[m_clickColumn] : std::string();
        if (after == m_cellBefore)
        {
            if (m_ticks < kClickWaitTicks)
                return;  // give EuroScope a moment
            logfile::Write(std::string("click with ") + rwydialog::MethodName(static_cast<rwydialog::ClickMethod>(m_method)) +
                           " had no effect");
            if (++m_failedClicks >= kTriesPerMethod)
            {
                m_failedClicks = 0;
                ++m_method;
            }
            if (m_method >= kMethodCount)
            {
                rwydialog::PressCancel(dialog);
                if (m_retried)
                {
                    Finish(false, "EuroScope ignored the clicks in the runway dialog");
                    return;
                }
                // Close the dialog and do the whole pass once more.
                logfile::Write("closing the runway dialog and trying again");
                m_retried = true;
                m_state = State::Reopening;
                m_ticks = 0;
                return;
            }
        }
        else
            m_failedClicks = 0;
        m_waitingForClick = false;
    }

    m_missing.clear();
    for (const auto& target : m_targets)
    {
        bool found = false;
        for (const auto& r : rows)
        {
            if (r.airport != target.icao || r.runway.empty())
                continue;
            found = true;
            // Only the runway columns: airports are activated by another plugin.
            const std::pair<int, bool> wanted[] = {
                {rwydialog::kColRunwayDep, target.departures.count(r.runway) > 0},
                {rwydialog::kColRunwayArr, target.arrivals.count(r.runway) > 0},
            };
            for (const auto& [column, on] : wanted)
            {
                if (rwydialog::IsTicked(r.cells[column]) == on)
                    continue;
                if (++m_clicks > kMaxClicks)
                {
                    rwydialog::PressCancel(dialog);
                    Finish(false, "too many clicks without reaching the configuration");
                    return;
                }
                m_waitingForClick = true;
                m_clickRow = r.index;
                m_clickColumn = column;
                m_cellBefore = r.cells[column];
                m_ticks = 0;
                rwydialog::Click(list, r.index, column, static_cast<rwydialog::ClickMethod>(m_method));
                return;  // check the effect on the next tick
            }
        }
        if (!found)
            m_missing.push_back(target.icao);
    }

    rwydialog::PressOk(dialog);
    m_state = State::Closing;
    m_ticks = 0;
}
