#include "pch.h"
#include "WindowLinkedResize.h"

#include <common/logger/logger.h>
#include <common/utils/elevation.h>
#include <common/utils/winapi_error.h>

#include <FancyZonesLib/FancyZonesData/CustomLayouts.h>
#include <FancyZonesLib/GridTracks.h>
#include <FancyZonesLib/Layout.h>
#include <FancyZonesLib/LinkedResizing.h>
#include <FancyZonesLib/Settings.h>
#include <FancyZonesLib/WindowUtils.h>
#include <FancyZonesLib/WorkArea.h>

namespace
{
    // Keep both zones and the grid tracks they may span large enough to remain
    // visible and draggable. GridTracks still permits an already-smaller track
    // to move in the direction that repairs it.
    constexpr LONG kMinZoneExtent = 32;

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
    constexpr UINT kDeferredApplyFlags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER;

    void ApplyRect(HWND window, const RECT& rect) noexcept
    {
        if (!SetWindowPos(window, nullptr, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, kApplyFlags))
        {
            Logger::error(L"Linked resize: SetWindowPos failed, {}", get_last_error_or_default(GetLastError()));
        }
    }

    bool AllowsTarget(HWND window, const std::optional<WindowSizeConstraints>& constraints, const RECT& target) noexcept
    {
        return !constraints.has_value() || !constraints->Matches(window) || constraints->Allows(target);
    }

    ZoneIndexSet ChangedZones(const ZonesMap& before, const ZonesMap& after)
    {
        ZoneIndexSet changed;
        for (const auto& [id, zone] : after)
        {
            const auto prior = before.find(id);
            if (prior == before.end())
            {
                changed.push_back(id);
                continue;
            }

            const RECT priorRect = prior->second.GetZoneRect();
            const RECT nextRect = zone.GetZoneRect();
            if (!EqualRect(&priorRect, &nextRect))
            {
                changed.push_back(id);
            }
        }

        return changed;
    }
}

WindowLinkedResize::WindowLinkedResize(HWND window, const RECT& startRect, WorkArea* workArea, Layout* layout, ZoneIndexSet windowZones, const ZonesMap& startZones, int gap, bool previewMode) :
    m_window(window),
    m_startRect(startRect),
    m_workArea(workArea),
    m_layout(layout),
    m_windowZones(std::move(windowZones)),
    m_startZones(startZones),
    m_gap(gap),
    m_previewMode(previewMode)
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

        auto session = std::unique_ptr<WindowLinkedResize>(new WindowLinkedResize(window,
                                                                                 startRect,
                                                                                 workArea.get(),
                                                                                 layout.get(),
                                                                                 windowZones,
                                                                                 layout->Zones(),
                                                                                 layout->TrackSpacing(),
                                                                                 FancyZonesSettings::settings().linkedResizePreview));
        session->m_windowConstraints = assignedWindows.GetWindowSizeConstraints(window);

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
            peerState.constraints = assignedWindows.GetWindowSizeConstraints(peer);
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

    // Live mode advances the effective layout on every update. Preview mode
    // advances only its private candidate map, leaving every peer window and
    // the persisted layout untouched until End().
    const ZonesMap& currentZones = m_previewMode && m_previewZones.has_value() ? m_previewZones.value() : layout->Zones();
    const RECT combined = Layout::CombinedZonesRect(currentZones, m_windowZones);
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
    ZonesMap movedZones = currentZones;
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
    const RECT activeTarget = FancyZonesWindowUtils::AdjustRectForSizeWindowToRect(m_window, Layout::CombinedZonesRect(movedZones, m_windowZones), workAreaWindow);
    if (!LinkedResizing::IsValidTargetRect(activeTarget) || !AllowsTarget(m_window, m_windowConstraints, activeTarget))
    {
        return;
    }

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
        if (!LinkedResizing::IsValidTargetRect(target) || !AllowsTarget(peer.window, peer.constraints, target))
        {
            return;
        }

        targets.push_back(target);
        preStepRects.push_back(peer.appliedRect);
    }

    if (m_previewMode)
    {
        m_previewZones = std::move(movedZones);
        m_workArea->ShowZonesPreview(m_previewZones.value(), ChangedZones(m_startZones, m_previewZones.value()), m_window);
        m_appliedDeltas = deltas;
        m_layoutChanged = true;
        return;
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
        if (Layout* layout = CurrentLayout())
        {
            if (m_previewMode)
            {
                if (!CommitPreview(*layout))
                {
                    Release();
                    return;
                }
            }
            else if (IsWindow(m_window))
            {
                const RECT target = FancyZonesWindowUtils::AdjustRectForSizeWindowToRect(m_window, layout->GetCombinedZonesRect(m_windowZones), m_workArea->GetWorkAreaWindow());
                RECT current{};
                if (LinkedResizing::IsValidTargetRect(target) && GetWindowRect(m_window, &current) && !EqualRect(&current, &target))
                {
                    ApplyRect(m_window, target);
                }
            }

            // A successfully completed gesture also persists the adjusted
            // topology of an applied custom grid layout.
            PersistAdjustedLayout(*layout);
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

    if (m_previewMode && IsWindow(m_window))
    {
        ApplyRect(m_window, m_startRect);
    }

    Release();
}

bool WindowLinkedResize::CommitPreview(Layout& layout) noexcept
{
    const auto rollback = [&]() noexcept {
        layout.ReplaceZones(m_startZones);
        if (IsWindow(m_window))
        {
            ApplyRect(m_window, m_startRect);
        }
        for (const auto& peer : m_peers)
        {
            if (IsWindow(peer.window))
            {
                ApplyRect(peer.window, peer.startRect);
            }
        }
    };

    if (!m_previewZones.has_value() || !m_workArea || !IsWindow(m_window))
    {
        rollback();
        return false;
    }

    struct WindowTarget
    {
        HWND window{};
        RECT target{};
        const std::optional<WindowSizeConstraints>* constraints{};
    };

    const HWND workAreaWindow = m_workArea->GetWorkAreaWindow();
    std::vector<WindowTarget> windows;
    windows.reserve(m_peers.size() + 1);
    windows.push_back(WindowTarget{
        .window = m_window,
        .target = FancyZonesWindowUtils::AdjustRectForSizeWindowToRect(m_window, Layout::CombinedZonesRect(m_previewZones.value(), m_windowZones), workAreaWindow),
        .constraints = &m_windowConstraints,
    });
    for (const auto& peer : m_peers)
    {
        windows.push_back(WindowTarget{
            .window = peer.window,
            .target = FancyZonesWindowUtils::AdjustRectForSizeWindowToRect(peer.window, Layout::CombinedZonesRect(m_previewZones.value(), peer.zones), workAreaWindow),
            .constraints = &peer.constraints,
        });
    }

    for (const auto& entry : windows)
    {
        if (!IsResizablePeer(entry.window) || !LinkedResizing::IsValidTargetRect(entry.target) ||
            !AllowsTarget(entry.window, *entry.constraints, entry.target))
        {
            rollback();
            return false;
        }
    }

    if (!layout.ReplaceZones(m_previewZones.value()))
    {
        rollback();
        return false;
    }

    HDWP deferred = BeginDeferWindowPos(static_cast<int>(windows.size()));
    if (!deferred)
    {
        rollback();
        return false;
    }

    for (const auto& entry : windows)
    {
        deferred = DeferWindowPos(deferred,
                                  entry.window,
                                  nullptr,
                                  entry.target.left,
                                  entry.target.top,
                                  entry.target.right - entry.target.left,
                                  entry.target.bottom - entry.target.top,
                                  kDeferredApplyFlags);
        if (!deferred)
        {
            Logger::error(L"Linked resize preview: failed to prepare deferred window positions");
            rollback();
            return false;
        }
    }

    m_updating = true;
    const bool applied = EndDeferWindowPos(deferred) != FALSE;
    m_updating = false;
    bool exact = applied;
    for (const auto& entry : windows)
    {
        RECT actual{};
        if (!GetWindowRect(entry.window, &actual) || !EqualRect(&actual, &entry.target))
        {
            exact = false;
            break;
        }
    }

    if (!exact)
    {
        Logger::warn(L"Linked resize preview: a window rejected its proposed size; restoring the previous layout");
        rollback();
        return false;
    }

    for (size_t i = 0; i < m_peers.size(); ++i)
    {
        m_peers[i].appliedRect = windows[i + 1].target;
    }
    return true;
}

void WindowLinkedResize::PersistAdjustedLayout(Layout& layout) noexcept
{
    try
    {
        // Only an applied custom grid layout has a persisted topology to
        // update; every other layout type keeps the in-memory linked-resize
        // result.
        if (layout.Type() != FancyZonesDataTypes::ZoneSetLayoutType::Custom || !m_workArea)
        {
            return;
        }

        const auto customLayout = CustomLayouts::instance().GetCustomLayoutData(layout.Id());
        if (!customLayout.has_value() || customLayout->type != FancyZonesDataTypes::CustomLayoutType::Grid ||
            !std::holds_alternative<FancyZonesDataTypes::GridLayoutInfo>(customLayout->info))
        {
            return;
        }

        const auto& gridInfo = std::get<FancyZonesDataTypes::GridLayoutInfo>(customLayout->info);
        const auto percents = LayoutConfigurator::DeriveGridTrackPercents(layout.Zones(), gridInfo, m_workArea->GetWorkAreaRect(), layout.Spacing());
        if (!percents.has_value() ||
            !CustomLayouts::instance().SetGridLayoutTrackPercents(layout.Id(), percents->rowsPercents, percents->columnsPercents))
        {
            Logger::error(L"Linked resize: failed to persist the adjusted custom grid layout");
        }
    }
    catch (...)
    {
        Logger::error(L"Linked resize: failed to persist the adjusted custom grid layout");
    }
}

void WindowLinkedResize::Release() noexcept
{
    if (m_previewMode && m_workArea)
    {
        m_workArea->HideZones();
    }

    m_peers.clear();
    m_window = nullptr;
    m_workArea = nullptr;
    m_layout = nullptr;
    m_windowZones.clear();
    m_startZones.clear();
    m_previewZones.reset();
}
