#pragma once

#include <climits>
#include <optional>

#include <FancyZonesLib/Zone.h>

struct WindowSizeConstraints
{
    SIZE minimumTrackSize{};
    SIZE maximumTrackSize{ LONG_MAX, LONG_MAX };
    DWORD processId{};
    UINT dpi{};
    LONG_PTR style{};
    LONG_PTR extendedStyle{};

    bool Allows(const RECT& rect) const noexcept;
    bool Matches(HWND window) const noexcept;
};

class LayoutAssignedWindows
{
public :
    LayoutAssignedWindows() = default;
    ~LayoutAssignedWindows() = default;

    void Assign(HWND window, const ZoneIndexSet& zones);
    void Dismiss(HWND window);

    std::map<HWND, ZoneIndexSet> SnappedWindows() const noexcept;
    ZoneIndexSet GetZoneIndexSetFromWindow(HWND window) const noexcept;
    std::optional<WindowSizeConstraints> GetWindowSizeConstraints(HWND window) const noexcept;
    void RefreshWindowSizeConstraints(HWND window) noexcept;
    bool IsZoneEmpty(ZoneIndex zoneIndex) const noexcept;
    
    void CycleWindows(HWND window, bool reverse);

private:
    std::map<HWND, ZoneIndexSet> m_windowIndexSet{};
    std::map<ZoneIndexSet, std::vector<HWND>> m_windowsByIndexSets{};
    std::map<HWND, WindowSizeConstraints> m_windowSizeConstraints{};

    void InsertWindowIntoZone(HWND window, std::optional<size_t> tabSortKeyWithinZone, const ZoneIndexSet& indexSet);
    HWND GetNextZoneWindow(ZoneIndexSet indexSet, HWND current, bool reverse) noexcept;
};
