#include "pch.h"
#include "LayoutConfigurator.h"

#include <common/display/dpi_aware.h>
#include <common/logger/logger.h>

#include <FancyZonesLib/FancyZonesDataTypes.h>

namespace
{
    // PriorityGrid layout is unique for zoneCount <= 11. For zoneCount > 11 PriorityGrid is same as Grid
    FancyZonesDataTypes::GridLayoutInfo predefinedPriorityGridLayouts[11] = {
        /* 1 */
        FancyZonesDataTypes::GridLayoutInfo(FancyZonesDataTypes::GridLayoutInfo::Full{
            .rows = 1,
            .columns = 1,
            .rowsPercents = { 10000 },
            .columnsPercents = { 10000 },
            .cellChildMap = { { 0 } } }),
        /* 2 */
        FancyZonesDataTypes::GridLayoutInfo(FancyZonesDataTypes::GridLayoutInfo::Full{
            .rows = 1,
            .columns = 2,
            .rowsPercents = { 10000 },
            .columnsPercents = { 6667, 3333 },
            .cellChildMap = { { 0, 1 } } }),
        /* 3 */
        FancyZonesDataTypes::GridLayoutInfo(FancyZonesDataTypes::GridLayoutInfo::Full{
            .rows = 1,
            .columns = 3,
            .rowsPercents = { 10000 },
            .columnsPercents = { 2500, 5000, 2500 },
            .cellChildMap = { { 0, 1, 2 } } }),
        /* 4 */
        FancyZonesDataTypes::GridLayoutInfo(FancyZonesDataTypes::GridLayoutInfo::Full{
            .rows = 2,
            .columns = 3,
            .rowsPercents = { 5000, 5000 },
            .columnsPercents = { 2500, 5000, 2500 },
            .cellChildMap = { { 0, 1, 2 }, { 0, 1, 3 } } }),
        /* 5 */
        FancyZonesDataTypes::GridLayoutInfo(FancyZonesDataTypes::GridLayoutInfo::Full{
            .rows = 2,
            .columns = 3,
            .rowsPercents = { 5000, 5000 },
            .columnsPercents = { 2500, 5000, 2500 },
            .cellChildMap = { { 0, 1, 2 }, { 3, 1, 4 } } }),
        /* 6 */
        FancyZonesDataTypes::GridLayoutInfo(FancyZonesDataTypes::GridLayoutInfo::Full{
            .rows = 3,
            .columns = 3,
            .rowsPercents = { 3333, 3334, 3333 },
            .columnsPercents = { 2500, 5000, 2500 },
            .cellChildMap = { { 0, 1, 2 }, { 0, 1, 3 }, { 4, 1, 5 } } }),
        /* 7 */
        FancyZonesDataTypes::GridLayoutInfo(FancyZonesDataTypes::GridLayoutInfo::Full{
            .rows = 3,
            .columns = 3,
            .rowsPercents = { 3333, 3334, 3333 },
            .columnsPercents = { 2500, 5000, 2500 },
            .cellChildMap = { { 0, 1, 2 }, { 3, 1, 4 }, { 5, 1, 6 } } }),
        /* 8 */
        FancyZonesDataTypes::GridLayoutInfo(FancyZonesDataTypes::GridLayoutInfo::Full{
            .rows = 3,
            .columns = 4,
            .rowsPercents = { 3333, 3334, 3333 },
            .columnsPercents = { 2500, 2500, 2500, 2500 },
            .cellChildMap = { { 0, 1, 2, 3 }, { 4, 1, 2, 5 }, { 6, 1, 2, 7 } } }),
        /* 9 */
        FancyZonesDataTypes::GridLayoutInfo(FancyZonesDataTypes::GridLayoutInfo::Full{
            .rows = 3,
            .columns = 4,
            .rowsPercents = { 3333, 3334, 3333 },
            .columnsPercents = { 2500, 2500, 2500, 2500 },
            .cellChildMap = { { 0, 1, 2, 3 }, { 4, 1, 2, 5 }, { 6, 1, 7, 8 } } }),
        /* 10 */
        FancyZonesDataTypes::GridLayoutInfo(FancyZonesDataTypes::GridLayoutInfo::Full{
            .rows = 3,
            .columns = 4,
            .rowsPercents = { 3333, 3334, 3333 },
            .columnsPercents = { 2500, 2500, 2500, 2500 },
            .cellChildMap = { { 0, 1, 2, 3 }, { 4, 1, 5, 6 }, { 7, 1, 8, 9 } } }),
        /* 11 */
        FancyZonesDataTypes::GridLayoutInfo(FancyZonesDataTypes::GridLayoutInfo::Full{
            .rows = 3,
            .columns = 4,
            .rowsPercents = { 3333, 3334, 3333 },
            .columnsPercents = { 2500, 2500, 2500, 2500 },
            .cellChildMap = { { 0, 1, 2, 3 }, { 4, 1, 5, 6 }, { 7, 8, 9, 10 } } }),
    };
}

bool AddZone(Zone zone, ZonesMap& zones) noexcept
{
    auto zoneId = zone.Id();
    if (zones.contains(zoneId))
    {
        return false;
    }

    zones.insert({ zoneId, std::move(zone) });
    return true;
}

ZonesMap CalculateGridZones(FancyZonesUtils::Rect workArea, FancyZonesDataTypes::GridLayoutInfo gridLayoutInfo, int spacing)
{
    ZonesMap zones;

    long totalWidth = workArea.width();
    long totalHeight = workArea.height();
    struct Info
    {
        long Extent;
        long Start;
        long End;
    };
    std::vector<Info> rowInfo(gridLayoutInfo.rows());
    std::vector<Info> columnInfo(gridLayoutInfo.columns());

    // Note: The expressions below are carefully written to
    // make the sum of all zones' sizes exactly total{Width|Height}
    int totalPercents = 0;
    for (int row = 0; row < gridLayoutInfo.rows(); row++)
    {
        rowInfo[row].Start = totalPercents * totalHeight / LayoutConfigurator::C_MULTIPLIER;
        totalPercents += gridLayoutInfo.rowsPercents()[row];
        rowInfo[row].End = totalPercents * totalHeight / LayoutConfigurator::C_MULTIPLIER;
        rowInfo[row].Extent = rowInfo[row].End - rowInfo[row].Start;
    }

    totalPercents = 0;
    for (int col = 0; col < gridLayoutInfo.columns(); col++)
    {
        columnInfo[col].Start = totalPercents * totalWidth / LayoutConfigurator::C_MULTIPLIER;
        totalPercents += gridLayoutInfo.columnsPercents()[col];
        columnInfo[col].End = totalPercents * totalWidth / LayoutConfigurator::C_MULTIPLIER;
        columnInfo[col].Extent = columnInfo[col].End - columnInfo[col].Start;
    }

    for (int64_t row = 0; row < gridLayoutInfo.rows(); row++)
    {
        for (int64_t col = 0; col < gridLayoutInfo.columns(); col++)
        {
            int i = gridLayoutInfo.cellChildMap()[row][col];
            if (((row == 0) || (gridLayoutInfo.cellChildMap()[row - 1][col] != i)) &&
                ((col == 0) || (gridLayoutInfo.cellChildMap()[row][col - 1] != i)))
            {
                long left = columnInfo[col].Start;
                long top = rowInfo[row].Start;

                int64_t maxRow = row;
                while (((maxRow + 1) < gridLayoutInfo.rows()) && (gridLayoutInfo.cellChildMap()[maxRow + 1][col] == i))
                {
                    maxRow++;
                }
                int64_t maxCol = col;
                while (((maxCol + 1) < gridLayoutInfo.columns()) && (gridLayoutInfo.cellChildMap()[row][maxCol + 1] == i))
                {
                    maxCol++;
                }

                long right = columnInfo[maxCol].End;
                long bottom = rowInfo[maxRow].End;

                top += row == 0 ? spacing : spacing / 2;
                bottom -= maxRow == static_cast<int64_t>(gridLayoutInfo.rows()) - 1 ? spacing : spacing / 2;
                left += col == 0 ? spacing : spacing / 2;
                right -= maxCol == static_cast<int64_t>(gridLayoutInfo.columns()) - 1 ? spacing : spacing / 2;

                Zone zone(RECT{ left, top, right, bottom }, i);
                if (zone.IsValid())
                {
                    if (!AddZone(zone, zones))
                    {
                        Logger::error(L"Failed to create grid layout. Invalid zone id");
                        return {};
                    }
                }
                else
                {
                    // All zones within zone set should be valid in order to use its functionality.
                    Logger::error(L"Failed to create grid layout. Invalid zone");
                    return {};
                }
            }
        }
    }

    return zones;
}

ZonesMap LayoutConfigurator::Focus(FancyZonesUtils::Rect workArea, int zoneCount) noexcept
{
    ZonesMap zones;

    long left{ 100 };
    long top{ 100 };
    long right{ left + static_cast<long>(workArea.width() * 0.4) };
    long bottom{ top + static_cast<long>(workArea.height() * 0.4) };

    RECT focusZoneRect{ left, top, right, bottom };

    long focusRectXIncrement = (zoneCount <= 1) ? 0 : 50;
    long focusRectYIncrement = (zoneCount <= 1) ? 0 : 50;

    for (int i = 0; i < zoneCount; i++)
    {
        Zone zone(focusZoneRect, zones.size());
        if (zone.IsValid())
        {
            if (!AddZone(zone, zones))
            {
                Logger::error(L"Failed to create Focus layout. Invalid zone id");
                return {};
            }
        }
        else
        {
            // All zones within zone set should be valid in order to use its functionality.
            Logger::error(L"Failed to create Focus layout. Invalid zone");
            return {};
        }

        focusZoneRect.left += focusRectXIncrement;
        focusZoneRect.right += focusRectXIncrement;
        focusZoneRect.bottom += focusRectYIncrement;
        focusZoneRect.top += focusRectYIncrement;
    }

    return zones;
}

ZonesMap LayoutConfigurator::Rows(FancyZonesUtils::Rect workArea, int zoneCount, int spacing) noexcept
{
    if (zoneCount == 0)
    {
        return {};
    }

    ZonesMap zones;
    
    long totalWidth = workArea.width() - (spacing * 2);
    long totalHeight = workArea.height() - (spacing * (zoneCount + 1));

    long top = spacing;
    long left = spacing;
    long bottom;
    long right;

    // Note: The expressions below are NOT equal to total{Width|Height} / zoneCount and are done
    // like this to make the sum of all zones' sizes exactly total{Width|Height}.
    for (int zoneIndex = 0; zoneIndex < zoneCount; ++zoneIndex)
    {
        right = totalWidth + spacing;
        bottom = top + (zoneIndex + 1) * totalHeight / zoneCount - zoneIndex * totalHeight / zoneCount;

        Zone zone(RECT{ left, top, right, bottom }, zones.size());
        if (zone.IsValid())
        {
            if (!AddZone(zone, zones))
            {
                Logger::error(L"Failed to create Rows layout. Invalid zone id");
                return {};
            }
        }
        else
        {
            // All zones within zone set should be valid in order to use its functionality.
            Logger::error(L"Failed to create Rows layout. Invalid zone");
            return {};
        }

        top = bottom + spacing;
    }

    return zones;
}

ZonesMap LayoutConfigurator::Columns(FancyZonesUtils::Rect workArea, int zoneCount, int spacing) noexcept
{
    if (zoneCount == 0)
    {
        return {};
    }

    ZonesMap zones;

    long totalWidth = workArea.width() - (spacing * (zoneCount + 1));
    long totalHeight = workArea.height() - (spacing * 2);

    long top = spacing;
    long left = spacing;
    long bottom;
    long right;

    // Note: The expressions below are NOT equal to total{Width|Height} / zoneCount and are done
    // like this to make the sum of all zones' sizes exactly total{Width|Height}.
    for (int zoneIndex = 0; zoneIndex < zoneCount; ++zoneIndex)
    {
        right = left + (zoneIndex + 1) * totalWidth / zoneCount - zoneIndex * totalWidth / zoneCount;
        bottom = totalHeight + spacing;

        Zone zone(RECT{ left, top, right, bottom }, zones.size());
        if (zone.IsValid())
        {
            if (!AddZone(zone, zones))
            {
                Logger::error(L"Failed to create Columns layout. Invalid zone id");
                return {};
            }
        }
        else
        {
            // All zones within zone set should be valid in order to use its functionality.
            Logger::error(L"Failed to create Columns layout. Invalid zone");
            return {};
        }

        left = right + spacing;
    }

    return zones;
}

ZonesMap LayoutConfigurator::Grid(FancyZonesUtils::Rect workArea, int zoneCount, int spacing) noexcept
{
    if (zoneCount == 0)
    {
        return {};
    }

    int rows = 1, columns = 1;
    while (zoneCount / rows >= rows)
    {
        rows++;
    }
    rows--;
    columns = zoneCount / rows;
    if (zoneCount % rows == 0)
    {
        // even grid
    }
    else
    {
        columns++;
    }

    FancyZonesDataTypes::GridLayoutInfo gridLayoutInfo(FancyZonesDataTypes::GridLayoutInfo::Minimal{ .rows = rows, .columns = columns });

    // Note: The expressions below are NOT equal to C_MULTIPLIER / {rows|columns} and are done
    // like this to make the sum of all percents exactly C_MULTIPLIER
    for (int row = 0; row < rows; row++)
    {
        gridLayoutInfo.rowsPercents()[row] = C_MULTIPLIER * (row + 1) / rows - C_MULTIPLIER * row / rows;
    }
    for (int col = 0; col < columns; col++)
    {
        gridLayoutInfo.columnsPercents()[col] = C_MULTIPLIER * (col + 1) / columns - C_MULTIPLIER * col / columns;
    }

    for (int i = 0; i < rows; ++i)
    {
        gridLayoutInfo.cellChildMap()[i] = std::vector<int>(columns);
    }

    int index = 0;
    for (int row = 0; row < rows; row++)
    {
        for (int col = 0; col < columns; col++)
        {
            gridLayoutInfo.cellChildMap()[row][col] = index++;
            if (index == zoneCount)
            {
                index--;
            }
        }
    }

    return CalculateGridZones(workArea, gridLayoutInfo, spacing);
}

ZonesMap LayoutConfigurator::PriorityGrid(FancyZonesUtils::Rect workArea, int zoneCount, int spacing) noexcept
{
    if (zoneCount <= 0)
    {
        return {};
    }

    constexpr int predefinedLayoutsCount = sizeof(predefinedPriorityGridLayouts) / sizeof(FancyZonesDataTypes::GridLayoutInfo);
    if (zoneCount < predefinedLayoutsCount)
    {
        return CalculateGridZones(workArea, predefinedPriorityGridLayouts[zoneCount - 1], spacing); 
    }

    return Grid(workArea, zoneCount, spacing);
}

ZonesMap LayoutConfigurator::Custom(FancyZonesUtils::Rect workArea, HMONITOR monitor, const FancyZonesDataTypes::CustomLayoutData& zoneSet, int spacing) noexcept
{
    if (zoneSet.type == FancyZonesDataTypes::CustomLayoutType::Canvas && std::holds_alternative<FancyZonesDataTypes::CanvasLayoutInfo>(zoneSet.info))
    {
        ZonesMap zones;
        const auto& zoneSetInfo = std::get<FancyZonesDataTypes::CanvasLayoutInfo>(zoneSet.info);

        float width = static_cast<float>(workArea.width());
        float height = static_cast<float>(workArea.height());

        DPIAware::InverseConvert(monitor, width, height);

        for (const auto& zone : zoneSetInfo.zones)
        {
            float x = static_cast<float>(zone.x) * width / zoneSetInfo.lastWorkAreaWidth;
            float y = static_cast<float>(zone.y) * height / zoneSetInfo.lastWorkAreaHeight;
            float zoneWidth = static_cast<float>(zone.width) * width / zoneSetInfo.lastWorkAreaWidth;
            float zoneHeight = static_cast<float>(zone.height) * height / zoneSetInfo.lastWorkAreaHeight;

            DPIAware::Convert(monitor, x, y);
            DPIAware::Convert(monitor, zoneWidth, zoneHeight);
            
            Zone zone_to_add(RECT{ static_cast<long>(x), static_cast<long>(y), static_cast<long>(x + zoneWidth), static_cast<long>(y + zoneHeight) }, zones.size());
            if (zone_to_add.IsValid())
            {
                if (!AddZone(zone_to_add, zones))
                {
                    Logger::error(L"Failed to create Custom layout. Invalid zone id");
                    return {};
                }
            }
            else
            {
                // All zones within zone set should be valid in order to use its functionality.
                Logger::error(L"Failed to create Custom layout. Invalid zone");
                return {};
            }
        }

        return zones;
    }
    else if (zoneSet.type == FancyZonesDataTypes::CustomLayoutType::Grid && std::holds_alternative<FancyZonesDataTypes::GridLayoutInfo>(zoneSet.info))
    {
        const auto& info = std::get<FancyZonesDataTypes::GridLayoutInfo>(zoneSet.info);
        return CalculateGridZones(workArea, info, spacing);
    }

    return {};
}

std::optional<LayoutConfigurator::GridTrackPercents> LayoutConfigurator::DeriveGridTrackPercents(const ZonesMap& zones, const FancyZonesDataTypes::GridLayoutInfo& gridLayoutInfo, FancyZonesUtils::Rect workArea, int spacing) noexcept
{
    const int rows = gridLayoutInfo.rows();
    const int columns = gridLayoutInfo.columns();
    const std::vector<int>& rowsPercents = gridLayoutInfo.rowsPercents();
    const std::vector<int>& columnsPercents = gridLayoutInfo.columnsPercents();
    const std::vector<std::vector<int>>& cellChildMap = gridLayoutInfo.cellChildMap();
    const int64_t totalWidth = workArea.width();
    const int64_t totalHeight = workArea.height();

    const auto validPercents = [](const std::vector<int>& percents, int tracks) {
        if (static_cast<int>(percents.size()) != tracks)
        {
            return false;
        }

        int64_t sum = 0;
        for (const int percent : percents)
        {
            if (percent <= 0)
            {
                return false;
            }
            sum += percent;
        }

        return sum == C_MULTIPLIER;
    };

    if (rows <= 0 || columns <= 0 || totalWidth <= 0 || totalHeight <= 0 ||
        !validPercents(rowsPercents, rows) || !validPercents(columnsPercents, columns) ||
        static_cast<int>(cellChildMap.size()) != rows)
    {
        return std::nullopt;
    }

    // Per-zone cell span, recovered by walking the cell-child map the same way
    // CalculateGridZones does: a zone's top-left cell is one where neither the
    // cell above nor the cell to the left belongs to the same zone.
    struct CellSpan
    {
        int row{};
        int col{};
        int maxRow{};
        int maxCol{};
    };

    std::map<ZoneIndex, CellSpan> spans;
    for (int row = 0; row < rows; ++row)
    {
        if (static_cast<int>(cellChildMap[row].size()) != columns)
        {
            return std::nullopt;
        }

        for (int col = 0; col < columns; ++col)
        {
            const int id = cellChildMap[row][col];
            if (id < 0)
            {
                return std::nullopt;
            }

            const bool isZoneStart = (row == 0 || cellChildMap[static_cast<size_t>(row) - 1][col] != id) &&
                                     (col == 0 || cellChildMap[row][static_cast<size_t>(col) - 1] != id);
            if (!isZoneStart)
            {
                continue;
            }

            if (spans.contains(id))
            {
                // Disconnected cells of one id are not a rectangular zone.
                return std::nullopt;
            }

            CellSpan span{ .row = row, .col = col, .maxRow = row, .maxCol = col };
            while (static_cast<int64_t>(span.maxRow) + 1 < rows && cellChildMap[static_cast<size_t>(span.maxRow) + 1][col] == id)
            {
                ++span.maxRow;
            }
            while (static_cast<int64_t>(span.maxCol) + 1 < columns && cellChildMap[row][static_cast<size_t>(span.maxCol) + 1] == id)
            {
                ++span.maxCol;
            }

            spans.emplace(id, span);
        }
    }

    // The zones map and the cell-child map must describe the same zone set.
    if (spans.size() != zones.size())
    {
        return std::nullopt;
    }

    for (const auto& [id, span] : spans)
    {
        const auto zone = zones.find(id);
        if (zone == zones.end() || !zone->second.IsValid())
        {
            return std::nullopt;
        }
    }

    // Every cell must be covered by its zone's span: a cell outside of it means
    // the zone is not rectangular and cannot be reproduced.
    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < columns; ++col)
        {
            const CellSpan& span = spans.at(cellChildMap[row][col]);
            if (row < span.row || row > span.maxRow || col < span.col || col > span.maxCol)
            {
                return std::nullopt;
            }
        }
    }

    const auto cumulativeOf = [](const std::vector<int>& percents) {
        std::vector<int64_t> cumulative(percents.size() + 1, 0);
        for (size_t i = 0; i < percents.size(); ++i)
        {
            cumulative[i + 1] = cumulative[i] + percents[i];
        }
        return cumulative;
    };

    // For each internal grid line recover its position from the final zone
    // rectangles: a zone ending on the line has its leading edge spacing/2
    // below it, a zone starting on it has its trailing edge spacing/2 above.
    // Every visible segment of one line must agree; a line with no visible
    // segment keeps its original cumulative boundary.
    const auto deriveCumulative = [&](bool horizontal, int tracks, int64_t totalExtent, const std::vector<int64_t>& original) -> std::optional<std::vector<int64_t>> {
        std::vector<int64_t> cumulative(static_cast<size_t>(tracks) + 1);
        cumulative[0] = 0;
        cumulative[tracks] = C_MULTIPLIER;

        for (int boundary = 1; boundary < tracks; ++boundary)
        {
            std::optional<int64_t> position;
            const auto merge = [&position](int64_t candidate) {
                if (position.has_value())
                {
                    return *position == candidate;
                }
                position = candidate;
                return true;
            };

            for (const auto& [id, span] : spans)
            {
                const RECT rect = zones.at(id).GetZoneRect();
                const int spanStart = horizontal ? span.row : span.col;
                const int spanEnd = horizontal ? span.maxRow : span.maxCol;
                const LONG trailing = horizontal ? rect.top : rect.left;
                const LONG leading = horizontal ? rect.bottom : rect.right;

                if (spanEnd == boundary - 1 && !merge(static_cast<int64_t>(leading) + static_cast<int64_t>(spacing) / 2))
                {
                    return std::nullopt;
                }
                if (spanStart == boundary && !merge(static_cast<int64_t>(trailing) - static_cast<int64_t>(spacing) / 2))
                {
                    return std::nullopt;
                }
            }

            if (!position.has_value())
            {
                cumulative[boundary] = original[boundary];
                continue;
            }

            // Prefer the original cumulative boundary when it reproduces the
            // same pixel line; otherwise take the smallest cumulative that
            // truncates to the observed position.
            if (original[boundary] * totalExtent / C_MULTIPLIER == *position)
            {
                cumulative[boundary] = original[boundary];
            }
            else
            {
                cumulative[boundary] = (*position * C_MULTIPLIER + totalExtent - 1) / totalExtent;
            }
        }

        return cumulative;
    };

    const auto rowCumulative = deriveCumulative(true, rows, totalHeight, cumulativeOf(rowsPercents));
    const auto columnCumulative = deriveCumulative(false, columns, totalWidth, cumulativeOf(columnsPercents));
    if (!rowCumulative.has_value() || !columnCumulative.has_value())
    {
        return std::nullopt;
    }

    // Strictly increasing cumulative boundaries keep every track positive and
    // the vector sum exactly C_MULTIPLIER.
    const auto toPercents = [](const std::vector<int64_t>& cumulative) -> std::optional<std::vector<int>> {
        std::vector<int> percents(cumulative.size() - 1);
        for (size_t i = 0; i < percents.size(); ++i)
        {
            const int64_t percent = cumulative[i + 1] - cumulative[i];
            if (percent <= 0 || percent > C_MULTIPLIER)
            {
                return std::nullopt;
            }
            percents[i] = static_cast<int>(percent);
        }
        return percents;
    };

    const auto derivedRows = toPercents(*rowCumulative);
    const auto derivedColumns = toPercents(*columnCumulative);
    if (!derivedRows.has_value() || !derivedColumns.has_value())
    {
        return std::nullopt;
    }

    // The derived vectors must reproduce the effective zones map exactly;
    // anything else is an invalid conversion and must not be persisted.
    FancyZonesDataTypes::GridLayoutInfo updated(gridLayoutInfo);
    updated.m_rowsPercents = *derivedRows;
    updated.m_columnsPercents = *derivedColumns;

    const ZonesMap recomputed = CalculateGridZones(workArea, updated, spacing);
    if (recomputed.size() != zones.size())
    {
        return std::nullopt;
    }

    for (const auto& [id, zone] : zones)
    {
        const auto iter = recomputed.find(id);
        if (iter == recomputed.end())
        {
            return std::nullopt;
        }

        const RECT expected = zone.GetZoneRect();
        const RECT actual = iter->second.GetZoneRect();
        if (expected.left != actual.left || expected.top != actual.top ||
            expected.right != actual.right || expected.bottom != actual.bottom)
        {
            return std::nullopt;
        }
    }

    return GridTrackPercents{ .rowsPercents = *derivedRows, .columnsPercents = *derivedColumns };
}
