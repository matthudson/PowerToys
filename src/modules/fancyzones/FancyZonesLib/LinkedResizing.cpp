#include "pch.h"
#include "LinkedResizing.h"

namespace
{
    // Positive when the intervals overlap; otherwise the negative distance between them.
    LONG IntervalOverlapOrGap(LONG aStart, LONG aEnd, LONG bStart, LONG bEnd) noexcept
    {
        if (bStart >= aEnd)
        {
            return aEnd - bStart;
        }

        if (aStart >= bEnd)
        {
            return bEnd - aStart;
        }

        return min(aEnd, bEnd) - max(aStart, bStart);
    }

    // True when the intervals overlap or are separated by at most maxGap. Used for the
    // axis perpendicular to a moved edge: a peer borders the resized window along that
    // edge when it faces it (overlap) or sits next to the moved corner (adjacent).
    bool OverlapsOrAdjacent(LONG aStart, LONG aEnd, LONG bStart, LONG bEnd, int maxGap) noexcept
    {
        return IntervalOverlapOrGap(aStart, aEnd, bStart, bEnd) >= -maxGap;
    }

    // True when the gap between two facing edges keeps the windows directly bordering:
    // separated by at most maxGap, or overlapping by at most kBorderSlack (invisible
    // window borders make window rects overlap even when the frames touch).
    bool EdgesAreAdjacent(LONG gap, int maxGap) noexcept
    {
        return gap >= -LinkedResizing::kBorderSlack && gap <= maxGap;
    }
}

LinkedResizing::EdgeDeltas LinkedResizing::ComputeEdgeDeltas(const RECT& before, const RECT& after) noexcept
{
    return EdgeDeltas{
        .left = after.left - before.left,
        .top = after.top - before.top,
        .right = after.right - before.right,
        .bottom = after.bottom - before.bottom,
    };
}

bool LinkedResizing::IsValidTargetRect(const RECT& rect) noexcept
{
    return rect.right > rect.left && rect.bottom > rect.top;
}

std::optional<RECT> LinkedResizing::ComputeLinkedPeerRect(const RECT& draggedBefore, const RECT& draggedAfter, const RECT& peerBefore, bool sameZone, int maxEdgeGap) noexcept
{
    if (sameZone)
    {
        // Windows stacked in the same zone set stay aligned with the resized window.
        return IsValidTargetRect(draggedAfter) ? std::optional<RECT>(draggedAfter) : std::nullopt;
    }

    const EdgeDeltas deltas = ComputeEdgeDeltas(draggedBefore, draggedAfter);

    // Gaps between the facing edges of both rects, positive when the peer sits beyond
    // the resized window's edge. Computed from the rects captured at gesture start so
    // repeated updates stay idempotent and keep working after earlier resize sessions
    // made the windows diverge from their zone rects.
    const LONG rightGap = peerBefore.left - draggedBefore.right;
    const LONG leftGap = draggedBefore.left - peerBefore.right;
    const LONG bottomGap = peerBefore.top - draggedBefore.bottom;
    const LONG topGap = draggedBefore.top - peerBefore.bottom;

    const auto adjacent = [maxEdgeGap](LONG gap) {
        return EdgesAreAdjacent(gap, maxEdgeGap);
    };

    const bool facesVertically = OverlapsOrAdjacent(peerBefore.top, peerBefore.bottom, draggedBefore.top, draggedBefore.bottom, maxEdgeGap);
    const bool facesHorizontally = OverlapsOrAdjacent(peerBefore.left, peerBefore.right, draggedBefore.left, draggedBefore.right, maxEdgeGap);

    // The link relation is fixed by the rects captured at gesture start: every edge
    // that directly borders the resized window follows that edge's current delta.
    // A delta that returns to zero restores the peer's original edge.
    RECT target = peerBefore;
    bool linked = false;

    if (facesVertically && adjacent(rightGap))
    {
        target.left += deltas.right;
        linked = true;
    }

    if (facesVertically && adjacent(leftGap))
    {
        target.right += deltas.left;
        linked = true;
    }

    if (facesHorizontally && adjacent(bottomGap))
    {
        target.top += deltas.bottom;
        linked = true;
    }

    if (facesHorizontally && adjacent(topGap))
    {
        target.bottom += deltas.top;
        linked = true;
    }

    if (!linked || !IsValidTargetRect(target))
    {
        return std::nullopt;
    }

    return target;
}
