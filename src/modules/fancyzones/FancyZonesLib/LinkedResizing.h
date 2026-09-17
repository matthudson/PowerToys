#pragma once

#include <optional>
#include <vector>

#include <windef.h>

// Pure geometry helpers for linked live resizing of zoned windows (issue #254).
// Everything in this namespace operates on plain RECT values and performs no
// window management calls, so the logic can be exercised by unit tests.
namespace LinkedResizing
{
    // Extra slack (in px) applied when detecting whether two windows directly border
    // each other. GetWindowRect includes the invisible resize borders, so windows that
    // visibly touch can report a slightly negative gap between their window rects.
    constexpr int kBorderSlack = 16;

    // Per-edge displacement of a rect between two points in time.
    struct EdgeDeltas
    {
        LONG left{};
        LONG top{};
        LONG right{};
        LONG bottom{};

        bool AnyMoved() const noexcept
        {
            return left != 0 || top != 0 || right != 0 || bottom != 0;
        }
    };

    EdgeDeltas ComputeEdgeDeltas(const RECT& before, const RECT& after) noexcept;

    bool IsValidTargetRect(const RECT& rect) noexcept;

    // Computes where a snapped peer should end up given that the resized window moved
    // from draggedBefore to draggedAfter. peerBefore is the peer's rect captured when
    // the resize gesture started, sameZone is true when the peer is assigned to the same
    // zone set as the resized window (stacked windows stay aligned), and maxEdgeGap is
    // the maximum separation between facing edges that still counts as directly
    // bordering (the configured zone spacing; kBorderSlack only tolerates the
    // small negative gap reported when invisible resize borders make touching
    // frames overlap).
    // Returns nullopt when the peer is not linked to a moved edge or corner, or when the
    // resulting rectangle is not a valid target.
    std::optional<RECT> ComputeLinkedPeerRect(const RECT& draggedBefore, const RECT& draggedAfter, const RECT& peerBefore, bool sameZone, int maxEdgeGap) noexcept;

    // One shared-boundary ("grid track") move selected for a resize step.
    // Horizontal boundaries are identified by their y coordinate and move zone
    // bottom/top edges; vertical boundaries by their x coordinate and move zone
    // right/left edges. coordinate is the boundary's low line in GridTracks
    // convention: edges facing the negative direction rest on it, edges facing
    // the positive direction rest `gap` above it.
    struct BoundaryMove
    {
        bool horizontal = false;
        LONG coordinate = 0;
        LONG delta = 0;
    };

    // Maps a change of the resized window's edges onto the shared layout
    // boundaries the edges should drag along, given the combined zone rect of
    // the window's assigned zone set and the configured gap between facing zone
    // edges.
    //
    // A dragged right/bottom edge moves the boundary its combined outer edge
    // rests on; a dragged left/top edge moves the facing boundary line `gap`
    // below the combined left/top edge. Opposite edges translating together (a
    // plain move) select no boundary, so moving a window never mutates the
    // layout. A corner drag selects both tracks.
    std::vector<BoundaryMove> SelectBoundaryMoves(const EdgeDeltas& deltas, const RECT& combinedZonesRect, LONG gap) noexcept;
}
