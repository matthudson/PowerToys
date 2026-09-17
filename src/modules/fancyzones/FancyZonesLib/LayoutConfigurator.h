#pragma once

#include <optional>
#include <vector>

#include <FancyZonesLib/Zone.h>
#include <FancyZonesLib/util.h>

// Mapping zone id to zone
using ZonesMap = std::map<ZoneIndex, Zone>;

namespace FancyZonesDataTypes
{
    struct CustomLayoutData;
    struct GridLayoutInfo;
}

class LayoutConfigurator
{
public:
    // Fixed-point multiplier of the grid track percent vectors: every vector
    // holds positive entries summing to exactly C_MULTIPLIER.
    static constexpr int C_MULTIPLIER = 10000;

    static ZonesMap Focus(FancyZonesUtils::Rect workArea, int zoneCount) noexcept;
    static ZonesMap Rows(FancyZonesUtils::Rect workArea, int zoneCount, int spacing) noexcept;
    static ZonesMap Columns(FancyZonesUtils::Rect workArea, int zoneCount, int spacing) noexcept;
    static ZonesMap Grid(FancyZonesUtils::Rect workArea, int zoneCount, int spacing) noexcept;
    static ZonesMap PriorityGrid(FancyZonesUtils::Rect workArea, int zoneCount, int spacing) noexcept;
    static ZonesMap Custom(FancyZonesUtils::Rect workArea, HMONITOR monitor, const FancyZonesDataTypes::CustomLayoutData& data, int spacing) noexcept;

    // Row/column percent vectors recovered from an effective zones map, e.g.
    // one adjusted by a linked-resize gesture.
    struct GridTrackPercents
    {
        std::vector<int> rowsPercents;
        std::vector<int> columnsPercents;
    };

    /**
     * Derives the row/column percent vectors that reproduce the given zones map
     * for a custom grid layout. Each visible internal grid line is recovered
     * from the final zone rectangles plus spacing and the cell-child map; a
     * completely hidden line keeps its original cumulative boundary. Returns
     * nullopt when the map cannot be reproduced from the layout topology: the
     * caller then keeps the in-memory result and writes nothing.
     */
    static std::optional<GridTrackPercents> DeriveGridTrackPercents(const ZonesMap& zones, const FancyZonesDataTypes::GridLayoutInfo& gridLayoutInfo, FancyZonesUtils::Rect workArea, int spacing) noexcept;
};
