#include "pch.h"
#include "WindowLinkedResize.h"

#include <common/logger/logger.h>
#include <common/utils/elevation.h>
#include <common/utils/winapi_error.h>

#include <FancyZonesLib/GridTracks.h>
#include <FancyZonesLib/Layout.h>
#include <FancyZonesLib/LinkedResizing.h>
#include <FancyZonesLib/WindowUtils.h>
#include <FancyZonesLib/WorkArea.h>

namespace
{
    // Boundary moves only require the moved zones to keep a positive extent.
    constexpr LONG kMinZoneExtent = 0;

    bool TryGetWindowRect(HWND window, RECT& rect) noexcept
    {
        return GetWindowRect(window, &rect) && rect.right > rect.left && rect.bottom > rect.top;
    }

    // Minimized, non-resizable or otherwise inaccessible peers are rejected.
    bool IsResizablePeer(HWND window) noexcept
    {
        if (!IsWindow(window) || IsIconic(window) || !IsWindowVisible(window))
        {
            return false;
        }

        return FancyZonesWindowUtils::HasStyle(GetWindowLong(window, GWL_STYLE), WS_SIZEBOX);
    }

    // SWP_NOZORDER/SWP_NOOWNERZORDER preserve the z-order, SWP_NOACTIVATE keeps the
    // resized window active, and SWP_ASYNCWINDOWPOS keeps a hung or inaccessible
    // peer from blocking the gesture.
    constexpr UINT kApplyFlags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_ASYNCWINDOWPOS;

    void ApplyRect(HWND window, const RECT& rect) noexcept
    {
        if (!SetWindowPos(window, nullptr, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, kApplyFlags))
        {
            Logger::error(L"Linked resize: SetWindowPos failed, {}", get_last_error_or_default(GetLastError()));
        }
    }
}

WindowLinkedResize::WindowLinkedResize(HWND window, const RECT& startRect, WorkArea* workArea, Layout* layout, ZoneIndexSet windowZones, const ZonesMap& startZones, int gap) :
    m_window(window),
    m_startRect(startRect),
    m_workArea(workArea),
    m_layout(layout),
    m_windowZones(std::move(windowZones)),
    m_startZones(startZones),
    m_gap(gap)
{
}

std::unique_ptr<WindowLinkedResize> WindowLinkedResize::Create(HWND window, const std::unordered_map<HMONITOR, std::unique_ptr<WorkArea>>& activeWorkAreas)
{
    if (!IsWindow(window) || IsIconic(window) || !IsResizablePeer(window))
    {
        return nullptr;
    }

    // Same convention as WindowMouseSnap::Create: an unelevated FancyZones cannot
    // control windows owned by elevated processes, so the session fails safe
    // instead of moving peers while the resized window stays put.
    const bool canMoveElevatedWindows = is_process_elevated();
    if (!canMoveElevatedWindows && IsProcessOfWindowElevated(window))
    {
        return nullptr;
    }

    RECT startRect{};
    if (!TryGetWindowRect(window, startRect))
    {
        return nullptr;
    }

    for (const auto& [_, workArea] : activeWorkAreas)
    {
        if (!workArea)
        {
            continue;
        }

        const auto& assignedWindows = workArea->GetLayoutWindows();
        const auto windowZones = assignedWindows.GetZoneIndexSetFromWindow(window);
        if (windowZones.empty())
        {
            continue;
        }

        const auto& layout = workArea->GetLayout();
        if (!layout || !LinkedResizing::IsValidTargetRect(layout->GetCombinedZonesRect(windowZones)))
        {
            continue;
        }

        auto session = std::unique_ptr<WindowLinkedResize>(new WindowLinkedResize(window, startRect, workArea.get(), layout.get(), windowZones, layout->Zones(), layout->Spacing()));

        for (const auto& [peer, peerZones] : assignedWindows.SnappedWindows())
        {
            if (peer == window)
            {
                continue;
            }

            // An assigned window that cannot follow the layout mutation fails
            // the whole session: silently skipping it would move its zones out
            // from under it and leave it behind. Elevation of a live process
            // cannot change mid-gesture, so checking it here keeps the
            // per-event Update() path cheap.
            if (peerZones.empty() || !IsResizablePeer(peer) ||
                (!canMoveElevatedWindows && IsProcessOfWindowElevated(peer)))
            {
                return nullptr;
            }

            Peer peerState{
                .window = peer,
                .zones = peerZones,
            };
            if (!TryGetWindowRect(peer, peerState.startRect))
            {
                return nullptr;
            }

            peerState.appliedRect = peerState.startRect;
            session->m_peers.push_back(std::move(peerState));
        }

        // Even without a following peer the session still tracks the resized
        // window: moving a boundary reshapes empty zones for future snapping.
        return session;
    }

    return nullptr;
}

Layout* WindowLinkedResize::CurrentLayout() const noexcept
{
    if (!m_workArea)
    {
        return nullptr;
    }

    const auto& layout = m_workArea->GetLayout();
    // A layout refresh mid-gesture replaces the Layout object; the session then
    // stops mutating zones instead of writing into a stale or unrelated map.
    return (layout && layout.get() == m_layout) ? m_layout : nullptr;
}

void WindowLinkedResize::Update(HWND window) noexcept
{
    if (m_updating || window != m_window || !IsWindow(m_window))
    {
        return;
    }

    RECT currentRect{};
    if (!GetWindowRect(m_window, &currentRect))
    {
        return;
    }

    const auto deltas = LinkedResizing::ComputeEdgeDeltas(m_startRect, currentRect);
    const LinkedResizing::EdgeDeltas pending{
        .left = deltas.left - m_appliedDeltas.left,
        .top = deltas.top - m_appliedDeltas.top,
        .right = deltas.right - m_appliedDeltas.right,
        .bottom = deltas.bottom - m_appliedDeltas.bottom,
    };
    if (!pending.AnyMoved())
    {
        return;
    }

    Layout* layout = CurrentLayout();
    if (!layout)
    {
        return;
    }

    // The moved boundary lines are derived from the resized window's assigned
    // zone set and the current effective zones, so every update starts from the
    // map produced by the previous one.
    const RECT combined = layout->GetCombinedZonesRect(m_windowZones);
    if (!LinkedResizing::IsValidTargetRect(combined))
    {
        return;
    }

    const auto moves = LinkedResizing::SelectBoundaryMoves(pending, combined, m_gap);
    if (moves.empty())
    {
        // A plain translation mutates no boundary; consume the deltas so the
        // next update only measures the remaining edge motion.
        m_appliedDeltas = deltas;
        return;
    }

    // Compose every selected track move on a copy of the effective map: a
    // rejected track leaves the previous layout and all peer rects unchanged.
    ZonesMap movedZones = layout->Zones();
    for (const auto& move : moves)
    {
        auto moved = move.horizontal
                         ? GridTracks::MoveHorizontalBoundary(movedZones, move.coordinate, move.delta, m_gap, kMinZoneExtent)
                         : GridTracks::MoveVerticalBoundary(movedZones, move.coordinate, move.delta, m_gap, kMinZoneExtent);
        if (!moved.has_value())
        {
            return;
        }

        movedZones = std::move(moved.value());
    }

    // Every peer target is prepared and validated against the prospective moved
    // map before the live layout is touched: an assigned window that can no
    // longer follow, or an invalid target, rejects the whole step so no window
    // is left behind by a mutated layout.
    HWND workAreaWindow = m_workArea->GetWorkAreaWindow();
    std::vector<RECT> targets;
    std::vector<RECT> preStepRects;
    targets.reserve(m_peers.size());
    preStepRects.reserve(m_peers.size());
    for (const auto& peer : m_peers)
    {
        if (!IsResizablePeer(peer.window))
        {
            return;
        }

        const RECT target = FancyZonesWindowUtils::AdjustRectForSizeWindowToRect(peer.window, Layout::CombinedZonesRect(movedZones, peer.zones), workAreaWindow);
        if (!LinkedResizing::IsValidTargetRect(target))
        {
            return;
        }

        targets.push_back(target);
        preStepRects.push_back(peer.appliedRect);
    }

    const ZonesMap priorZones = layout->Zones();
    if (!layout->ReplaceZones(std::move(movedZones)))
    {
        return;
    }

    // Every assigned window follows its unchanged zone set's new combined rect;
    // same-zone stacked windows share the zone set and stay aligned. The
    // actively dragged window stays cursor-driven until End() normalizes it.
    m_updating = true;
    for (size_t i = 0; i < m_peers.size(); ++i)
    {
        auto& peer = m_peers[i];
        const RECT& target = targets[i];
        if (EqualRect(&target, &peer.appliedRect))
        {
            continue;
        }

        if (SetWindowPos(peer.window, nullptr, target.left, target.top, target.right - target.left, target.bottom - target.top, kApplyFlags))
        {
            peer.appliedRect = target;
            continue;
        }

        Logger::error(L"Linked resize: SetWindowPos failed, {}", get_last_error_or_default(GetLastError()));

        // The live map was already replaced, so roll this step back
        // best-effort: restore the prior effective zones map and the peers
        // moved before the failure, and leave m_appliedDeltas unconsumed so the
        // next update retries the full pending motion.
        layout->ReplaceZones(priorZones);
        for (size_t j = 0; j < i; ++j)
        {
            auto& movedPeer = m_peers[j];
            if (!EqualRect(&movedPeer.appliedRect, &preStepRects[j]) && IsWindow(movedPeer.window))
            {
                ApplyRect(movedPeer.window, preStepRects[j]);
                movedPeer.appliedRect = preStepRects[j];
            }
        }

        m_updating = false;
        return;
    }
    m_updating = false;

    m_appliedDeltas = deltas;
    m_layoutChanged = true;
}

void WindowLinkedResize::End() noexcept
{
    Update(m_window);

    // The dragged window stayed cursor-driven during the gesture; settle it
    // onto its assigned zone set so the window and the effective layout agree.
    // Only a gesture that actually moved a grid track is normalized: a rejected
    // move (outer edge, unsupported boundary) or no boundary motion at all
    // leaves an ordinary single-window resize. When an earlier move was
    // accepted, this still clamps the window to the last accepted effective
    // layout even if the final requested move was rejected.
    if (m_layoutChanged)
    {
        if (Layout* layout = CurrentLayout(); layout && IsWindow(m_window))
        {
            const RECT target = FancyZonesWindowUtils::AdjustRectForSizeWindowToRect(m_window, layout->GetCombinedZonesRect(m_windowZones), m_workArea->GetWorkAreaWindow());
            RECT current{};
            if (LinkedResizing::IsValidTargetRect(target) && GetWindowRect(m_window, &current) && !EqualRect(&current, &target))
            {
                ApplyRect(m_window, target);
            }
        }
    }

    // End commits the in-memory effective layout produced during the gesture.
    Release();
}

void WindowLinkedResize::Cancel() noexcept
{
    // Roll the effective zones map back to its pre-gesture state, then put back
    // every peer this session moved.
    if (Layout* layout = CurrentLayout())
    {
        layout->ReplaceZones(m_startZones);
    }

    for (const auto& peer : m_peers)
    {
        if (EqualRect(&peer.appliedRect, &peer.startRect) || !IsWindow(peer.window))
        {
            continue;
        }

        ApplyRect(peer.window, peer.startRect);
    }

    Release();
}

void WindowLinkedResize::Release() noexcept
{
    m_peers.clear();
    m_window = nullptr;
    m_workArea = nullptr;
    m_layout = nullptr;
    m_windowZones.clear();
    m_startZones.clear();
}
