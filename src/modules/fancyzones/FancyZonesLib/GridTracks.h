#pragma once

#include <map>
#include <optional>
#include <vector>

#include <windef.h>

#include <FancyZonesLib/LayoutConfigurator.h> // ZonesMap

// Pure geometry primitive that moves a complete collinear shared boundary
// (a "grid track" line) inside an existing FancyZones zones map (issue #254).
//
// A horizontal boundary is identified by the y coordinate shared by the bottom
// edges of the zones above it; the facing top edges of the zones below sit at
// boundaryY + gap. Moving the boundary shifts every matching edge by the same
// delta, including disconnected visible boundary segments separated by zones
// that span the track, and preserves the configured gap between facing edges.
// Vertical boundaries behave symmetrically on zone right/left edges.
//
// The primitive operates on plain zone rectangles only: it performs no window
// management calls and is not wired into the layout lifecycle yet. Every
// function returns nullopt instead of a partially moved map when a move is
// rejected.
namespace GridTracks
{
    // Moves the horizontal shared boundary at boundaryY by deltaY: every zone
    // bottom at boundaryY and every zone top at boundaryY + gap move together.
    //
    // Returns nullopt when boundaryY does not identify a shared boundary (edges
    // present on only one side, e.g. an ambiguous outer edge of the map), when
    // stray zone edges make the boundary ambiguous (an edge resting on the
    // opposite boundary line or floating inside the gap band), or when the move
    // would produce a non-positive zone rect, shrink a moved zone below
    // minHeight, or place this cut closer than minHeight to another parallel
    // cut. An already-undersized zone or cut may move only toward recovery.
    std::optional<ZonesMap> MoveHorizontalBoundary(const ZonesMap& zones, LONG boundaryY, LONG deltaY, LONG gap, LONG minHeight) noexcept;

    // Symmetric vertical variant: every zone right at boundaryX and every zone
    // left at boundaryX + gap move together; minWidth applies to zone widths.
    std::optional<ZonesMap> MoveVerticalBoundary(const ZonesMap& zones, LONG boundaryX, LONG deltaX, LONG gap, LONG minWidth) noexcept;

    // Composes one horizontal and one vertical boundary move for a corner drag.
    // Returns nullopt when either move fails; no partially moved map is produced.
    std::optional<ZonesMap> MoveCornerBoundaries(const ZonesMap& zones, LONG boundaryY, LONG deltaY, LONG boundaryX, LONG deltaX, LONG gap, LONG minExtent) noexcept;
}
