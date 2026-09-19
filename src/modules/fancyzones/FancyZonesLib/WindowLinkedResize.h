#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include <FancyZonesLib/LayoutConfigurator.h> // ZonesMap
#include <FancyZonesLib/LayoutAssignedWindows.h> // WindowSizeConstraints
#include <FancyZonesLib/LinkedResizing.h> // EdgeDeltas

class Layout;
class WorkArea;

// Implements linked live resizing of zoned windows (issue #254): while a snapped
// window is being resized, the shared layout boundaries the dragged edges rest
// on move with them (GridTracks), and every window assigned to the work area is
// resized onto its unchanged zone set's new combined rect, keeping the
// configured gap between zones invariant.
//
// Boundary selection and zone-rect math are pure functions (LinkedResizing and
// GridTracks); this class only captures the window state when the gesture
// starts, applies the computed layouts and rects through Win32 during the
// gesture, and rolls everything back on cancel.
class WindowLinkedResize
{
    WindowLinkedResize(HWND window, const RECT& startRect, WorkArea* workArea, Layout* layout, ZoneIndexSet windowZones, const ZonesMap& startZones, int gap, bool previewMode);

public:
    static std::unique_ptr<WindowLinkedResize> Create(HWND window, const std::unordered_map<HMONITOR, std::unique_ptr<WorkArea>>& activeWorkAreas);

    HWND GetResizedWindow() const noexcept { return m_window; }

    // Recomputes the linked layout from a location-change event. Only reacts to
    // events for the resized window, which also prevents recursive feedback from
    // the location-change events generated when peers are resized.
    void Update(HWND window) noexcept;

    // Normalizes the resized window onto its assigned zone rect when the
    // gesture moved a grid track, commits the in-memory effective layout, then
    // stops tracking. A gesture that never mutated the layout (outer edge,
    // unsupported boundary, plain move) stays an ordinary single-window resize.
    void End() noexcept;

    // Restores the pre-gesture zones map and every peer this session moved,
    // then stops tracking (abort, setting disabled, ...).
    void Cancel() noexcept;

private:
    struct Peer
    {
        HWND window{};
        ZoneIndexSet zones{};   // assigned zone set; unchanged by the gesture
        RECT startRect{};       // window rect captured when the gesture started
        RECT appliedRect{};     // last rect applied through SetWindowPos
        std::optional<WindowSizeConstraints> constraints{};
    };

    // The layout captured when the gesture started, or null when the work
    // area's Layout object was replaced mid-gesture.
    Layout* CurrentLayout() const noexcept;

    // Persists the adjusted topology of an applied custom grid layout so a
    // reopened editor and a restarted FancyZones see the same grid. Other
    // layout types keep the in-memory linked-resize result only; a failed
    // conversion or an unavailable layout writes nothing.
    void PersistAdjustedLayout(Layout& layout) noexcept;

    // Applies a deferred preview as one batched window-position transaction.
    // Any rejected target restores every window and the original zone map.
    bool CommitPreview(Layout& layout) noexcept;

    void Release() noexcept;

    HWND m_window{};
    RECT m_startRect{};
    WorkArea* m_workArea{};
    Layout* m_layout{};
    ZoneIndexSet m_windowZones{};
    ZonesMap m_startZones{}; // effective zones map captured when the gesture started
    const int m_gap{};
    const bool m_previewMode{};
    std::vector<Peer> m_peers{};
    std::optional<WindowSizeConstraints> m_windowConstraints{};
    std::optional<ZonesMap> m_previewZones{};
    LinkedResizing::EdgeDeltas m_appliedDeltas{}; // cumulative edge deltas already folded into the layout
    bool m_layoutChanged{ false }; // at least one step committed a moved grid track to the effective layout
    bool m_updating{ false }; // reentrancy guard while applying peer positions
};
