#include "pch.h"
#include "GridTracks.h"

namespace
{
    // The two collinear edge lines forming a shared boundary: zone edges facing
    // the negative direction rest on low, zone edges facing the positive
    // direction rest on high, exactly `gap` apart. For a horizontal boundary
    // the facing edges are zone bottoms (low) and tops (high); vertical is
    // symmetric on rights and lefts.
    struct BoundaryLines
    {
        LONG low{};
        LONG high{};
    };

    // Strictly inside the open band between the two boundary lines.
    bool InsideGapBand(LONG edge, const BoundaryLines& lines) noexcept
    {
        const LONG lo = min(lines.low, lines.high);
        const LONG hi = max(lines.low, lines.high);
        return edge > lo && edge < hi;
    }

    // A zone can span several grid tracks, so checking zone extents alone does
    // not prevent two parallel cuts from being squeezed into a tiny, unusable
    // track. Keep the moving cut on the same side of every other cut and at
    // least minExtent away. If the loaded layout is already undersized, allow
    // movement only when it increases the deficient separation so the layout
    // can be repaired interactively.
    bool HasValidTrackSeparation(const ZonesMap& zones, bool horizontal, const BoundaryLines& lines, LONG delta, LONG minExtent) noexcept
    {
        if (delta == 0)
        {
            return true;
        }

        const LONG targetLow = lines.low + delta;
        const LONG targetHigh = lines.high + delta;

        const auto validAgainst = [minExtent](LONG current, LONG target, LONG other) noexcept {
            if (other == current)
            {
                return true;
            }

            const int64_t currentOffset = static_cast<int64_t>(current) - other;
            const int64_t targetOffset = static_cast<int64_t>(target) - other;
            if ((currentOffset < 0 && targetOffset >= 0) || (currentOffset > 0 && targetOffset <= 0))
            {
                return false; // do not cross or collapse another grid cut
            }

            const int64_t currentDistance = currentOffset < 0 ? -currentOffset : currentOffset;
            const int64_t targetDistance = targetOffset < 0 ? -targetOffset : targetOffset;
            return currentDistance < minExtent ? targetDistance > currentDistance : targetDistance >= minExtent;
        };

        for (const auto& [_, zone] : zones)
        {
            const RECT rect = zone.GetZoneRect();
            const LONG leading = horizontal ? rect.bottom : rect.right;
            const LONG trailing = horizontal ? rect.top : rect.left;
            if (!validAgainst(lines.low, targetLow, leading) || !validAgainst(lines.high, targetHigh, trailing))
            {
                return false;
            }
        }

        return true;
    }

    std::optional<ZonesMap> MoveBoundary(const ZonesMap& zones, bool horizontal, LONG coordinate, LONG delta, LONG gap, LONG minExtent) noexcept
    {
        const BoundaryLines lines{ coordinate, coordinate + gap };
        const LONG targetLow = lines.low + delta;
        const LONG targetHigh = lines.high + delta;
        const LONG minRequired = minExtent > 0 ? minExtent : 1;

        if (!HasValidTrackSeparation(zones, horizontal, lines, delta, minRequired))
        {
            return std::nullopt;
        }

        ZonesMap moved;
        size_t lowEdges = 0;
        size_t highEdges = 0;

        for (const auto& [id, zone] : zones)
        {
            const RECT rect = zone.GetZoneRect();
            const LONG leading = horizontal ? rect.bottom : rect.right; // edge facing the positive direction
            const LONG trailing = horizontal ? rect.top : rect.left;    // edge facing the negative direction

            // A zone edge resting on the opposite boundary line, or floating
            // inside the gap band, makes the boundary ambiguous.
            const bool onOppositeLine = gap != 0 && (trailing == lines.low || leading == lines.high);
            if (onOppositeLine || InsideGapBand(leading, lines) || InsideGapBand(trailing, lines))
            {
                return std::nullopt;
            }

            RECT updated = rect;
            const bool onLow = leading == lines.low;
            const bool onHigh = trailing == lines.high;
            if (onLow)
            {
                if (horizontal)
                {
                    updated.bottom = targetLow;
                }
                else
                {
                    updated.right = targetLow;
                }
                lowEdges++;
            }
            if (onHigh)
            {
                if (horizontal)
                {
                    updated.top = targetHigh;
                }
                else
                {
                    updated.left = targetHigh;
                }
                highEdges++;
            }

            if (onLow || onHigh)
            {
                const LONG extent = horizontal ? updated.bottom - updated.top : updated.right - updated.left;
                const LONG priorExtent = horizontal ? rect.bottom - rect.top : rect.right - rect.left;
                const bool extentIsValid = priorExtent < minRequired ? extent > priorExtent : extent >= minRequired;
                if (updated.right <= updated.left || updated.bottom <= updated.top || !extentIsValid)
                {
                    return std::nullopt;
                }
            }

            moved.emplace(id, Zone(updated, id));
        }

        // Edges on only one side mean the coordinate is an outer edge of the
        // map rather than a shared boundary.
        if (lowEdges == 0 || highEdges == 0)
        {
            return std::nullopt;
        }

        return moved;
    }
}

std::optional<ZonesMap> GridTracks::MoveHorizontalBoundary(const ZonesMap& zones, LONG boundaryY, LONG deltaY, LONG gap, LONG minHeight) noexcept
{
    return MoveBoundary(zones, true, boundaryY, deltaY, gap, minHeight);
}

std::optional<ZonesMap> GridTracks::MoveVerticalBoundary(const ZonesMap& zones, LONG boundaryX, LONG deltaX, LONG gap, LONG minWidth) noexcept
{
    return MoveBoundary(zones, false, boundaryX, deltaX, gap, minWidth);
}

std::optional<ZonesMap> GridTracks::MoveCornerBoundaries(const ZonesMap& zones, LONG boundaryY, LONG deltaY, LONG boundaryX, LONG deltaX, LONG gap, LONG minExtent) noexcept
{
    const auto horizontallyMoved = MoveHorizontalBoundary(zones, boundaryY, deltaY, gap, minExtent);
    if (!horizontallyMoved.has_value())
    {
        return std::nullopt;
    }

    return MoveVerticalBoundary(horizontallyMoved.value(), boundaryX, deltaX, gap, minExtent);
}
