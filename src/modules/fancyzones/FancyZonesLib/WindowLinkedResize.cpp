#include "pch.h"
#include "WindowLinkedResize.h"

#include <algorithm>

#include <common/logger/logger.h>
#include <common/utils/winapi_error.h>

#include <FancyZonesLib/Layout.h>
#include <FancyZonesLib/LinkedResizing.h>
#include <FancyZonesLib/WindowUtils.h>
#include <FancyZonesLib/WorkArea.h>

namespace
{
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
}

WindowLinkedResize::WindowLinkedResize(HWND window, const RECT& startRect, int maxEdgeGap) :
    m_window(window),
    m_startRect(startRect),
    m_maxEdgeGap(maxEdgeGap)
{
}

std::unique_ptr<WindowLinkedResize> WindowLinkedResize::Create(HWND window, const std::unordered_map<HMONITOR, std::unique_ptr<WorkArea>>& activeWorkAreas)
{
    if (!IsWindow(window) || IsIconic(window) || !IsResizablePeer(window))
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
        const int spacing = layout ? layout->Spacing() : 0;

        auto session = std::unique_ptr<WindowLinkedResize>(new WindowLinkedResize(window, startRect, spacing + LinkedResizing::kBorderSlack));

        for (const auto& [peer, peerZones] : assignedWindows.SnappedWindows())
        {
            if (peer == window || !IsResizablePeer(peer))
            {
                continue;
            }

            Peer peerState{
                .window = peer,
                .sameZone = peerZones.size() == windowZones.size() && std::is_permutation(peerZones.begin(), peerZones.end(), windowZones.begin()),
            };
            if (!TryGetWindowRect(peer, peerState.startRect))
            {
                continue;
            }

            peerState.appliedRect = peerState.startRect;
            session->m_peers.push_back(peerState);
        }

        // Linked resize only makes sense when at least one neighbor can follow.
        return session->m_peers.empty() ? nullptr : std::move(session);
    }

    return nullptr;
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

    // Skip the scan until the window's rect first changes; once peers were moved they
    // still need updates so edges that return to their start position get restored.
    if (!m_resizedPeers && !LinkedResizing::ComputeEdgeDeltas(m_startRect, currentRect).AnyMoved())
    {
        return;
    }

    m_updating = true;
    for (auto iter = m_peers.begin(); iter != m_peers.end();)
    {
        auto& peer = *iter;
        const auto target = LinkedResizing::ComputeLinkedPeerRect(m_startRect, currentRect, peer.startRect, peer.sameZone, m_maxEdgeGap);
        if (!target.has_value() || EqualRect(&target.value(), &peer.appliedRect))
        {
            ++iter;
            continue;
        }

        if (!IsResizablePeer(peer.window))
        {
            iter = m_peers.erase(iter);
            continue;
        }

        // SWP_NOZORDER/SWP_NOOWNERZORDER preserve the z-order, SWP_NOACTIVATE keeps the
        // resized window active, and SWP_ASYNCWINDOWPOS keeps a hung or inaccessible
        // peer from blocking the gesture.
        constexpr UINT flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_ASYNCWINDOWPOS;
        if (SetWindowPos(peer.window, nullptr, target->left, target->top, target->right - target->left, target->bottom - target->top, flags))
        {
            peer.appliedRect = *target;
            m_resizedPeers = true;
            ++iter;
        }
        else
        {
            Logger::error(L"Linked resize: SetWindowPos failed, {}", get_last_error_or_default(GetLastError()));
            iter = m_peers.erase(iter);
        }
    }
    m_updating = false;
}

void WindowLinkedResize::End() noexcept
{
    Update(m_window);
    Cancel();
}

void WindowLinkedResize::Cancel() noexcept
{
    m_peers.clear();
    m_window = nullptr;
}
