#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

class WorkArea;

// Implements linked live resizing of zoned windows (issue #254): while a snapped
// window is being resized, all snapped windows that directly border the moved
// edges or corners are resized along with it, keeping the configured gap between
// the windows invariant.
//
// All adjacency and target-rect math lives in LinkedResizing (LinkedResizing.h);
// this class only captures the window state when the gesture starts and applies
// the computed rects through Win32 during the gesture.
class WindowLinkedResize
{
    WindowLinkedResize(HWND window, const RECT& startRect, int maxEdgeGap);

public:
    static std::unique_ptr<WindowLinkedResize> Create(HWND window, const std::unordered_map<HMONITOR, std::unique_ptr<WorkArea>>& activeWorkAreas);

    HWND GetResizedWindow() const noexcept { return m_window; }

    // Recomputes the linked peers from a location-change event. Only reacts to
    // events for the resized window, which also prevents recursive feedback from
    // the location-change events generated when peers are resized.
    void Update(HWND window) noexcept;

    // Applies the final positions, then stops tracking.
    void End() noexcept;

    // Stops tracking without touching peer windows (abort, setting disabled, ...).
    void Cancel() noexcept;

private:
    struct Peer
    {
        HWND window{};
        RECT startRect{};   // window rect captured when the gesture started
        RECT appliedRect{}; // last rect applied through SetWindowPos
        bool sameZone = false;
    };

    HWND m_window{};
    RECT m_startRect{};
    const int m_maxEdgeGap{};
    std::vector<Peer> m_peers{};
    bool m_updating{ false };      // reentrancy guard while applying peer positions
    bool m_resizedPeers{ false };  // true once a peer has been moved from its start rect
};
