#include "pch.h"
#include "Util.h"

#include <FancyZonesLib/FancyZonesDataTypes.h>
#include <FancyZonesLib/GridTracks.h>
#include <FancyZonesLib/LayoutConfigurator.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace FancyZonesUnitTests
{
    TEST_CLASS (GridTrackPercentsUnitTest)
    {
        const FancyZonesUtils::Rect m_workArea{ RECT{ 0, 0, 1000, 800 } };

        static FancyZonesDataTypes::GridLayoutInfo GridInfo(
            int rows,
            int columns,
            const std::vector<int>& rowsPercents,
            const std::vector<int>& columnsPercents,
            const std::vector<std::vector<int>>& cellChildMap,
            int spacing = 0)
        {
            return FancyZonesDataTypes::GridLayoutInfo(FancyZonesDataTypes::GridLayoutInfo::Full{
                .rows = rows,
                .columns = columns,
                .rowsPercents = rowsPercents,
                .columnsPercents = columnsPercents,
                .cellChildMap = cellChildMap,
                .showSpacing = spacing != 0,
                .spacing = spacing,
                .sensitivityRadius = 20 });
        }

        static ZonesMap ZonesOf(const FancyZonesDataTypes::GridLayoutInfo& info, const FancyZonesUtils::Rect& workArea, int spacing)
        {
            FancyZonesDataTypes::CustomLayoutData layout{};
            layout.type = FancyZonesDataTypes::CustomLayoutType::Grid;
            layout.info = info;
            return LayoutConfigurator::Custom(workArea, nullptr, layout, spacing);
        }

        static void AssertInvariant(const LayoutConfigurator::GridTrackPercents& percents, int rows, int columns)
        {
            Assert::AreEqual(static_cast<size_t>(rows), percents.rowsPercents.size());
            Assert::AreEqual(static_cast<size_t>(columns), percents.columnsPercents.size());

            const auto sum = [](const std::vector<int>& vec) {
                int total = 0;
                for (const int value : vec)
                {
                    Assert::IsTrue(value > 0);
                    total += value;
                }
                return total;
            };

            Assert::AreEqual(LayoutConfigurator::C_MULTIPLIER, sum(percents.rowsPercents));
            Assert::AreEqual(LayoutConfigurator::C_MULTIPLIER, sum(percents.columnsPercents));
        }

        TEST_METHOD (HorizontalBoundaryMoveDerivesRowPercents)
        {
            const auto grid = GridInfo(2, 2, { 5000, 5000 }, { 5000, 5000 }, { { 0, 1 }, { 2, 3 } });
            const auto zones = ZonesOf(grid, m_workArea, 0);

            // The shared row boundary rests at y=400; a linked resize drags it to y=500.
            const auto moved = GridTracks::MoveHorizontalBoundary(zones, 400, 100, 0, 0);
            Assert::IsTrue(moved.has_value());

            const auto result = LayoutConfigurator::DeriveGridTrackPercents(moved.value(), grid, m_workArea, 0);
            Assert::IsTrue(result.has_value());
            Assert::AreEqual(std::vector<int>({ 6250, 3750 }), result->rowsPercents);
            Assert::AreEqual(std::vector<int>({ 5000, 5000 }), result->columnsPercents);
            AssertInvariant(result.value(), 2, 2);
        }

        TEST_METHOD (VerticalBoundaryMoveDerivesColumnPercents)
        {
            const auto grid = GridInfo(2, 2, { 5000, 5000 }, { 5000, 5000 }, { { 0, 1 }, { 2, 3 } });
            const auto zones = ZonesOf(grid, m_workArea, 0);

            // The shared column boundary rests at x=500.
            const auto moved = GridTracks::MoveVerticalBoundary(zones, 500, -50, 0, 0);
            Assert::IsTrue(moved.has_value());

            const auto result = LayoutConfigurator::DeriveGridTrackPercents(moved.value(), grid, m_workArea, 0);
            Assert::IsTrue(result.has_value());
            Assert::AreEqual(std::vector<int>({ 5000, 5000 }), result->rowsPercents);
            Assert::AreEqual(std::vector<int>({ 4500, 5500 }), result->columnsPercents);
        }

        TEST_METHOD (CornerMoveDerivesBothVectors)
        {
            const auto grid = GridInfo(2, 2, { 5000, 5000 }, { 5000, 5000 }, { { 0, 1 }, { 2, 3 } });
            const auto zones = ZonesOf(grid, m_workArea, 0);

            const auto moved = GridTracks::MoveCornerBoundaries(zones, 400, 100, 500, -50, 0, 0);
            Assert::IsTrue(moved.has_value());

            const auto result = LayoutConfigurator::DeriveGridTrackPercents(moved.value(), grid, m_workArea, 0);
            Assert::IsTrue(result.has_value());
            Assert::AreEqual(std::vector<int>({ 6250, 3750 }), result->rowsPercents);
            Assert::AreEqual(std::vector<int>({ 4500, 5500 }), result->columnsPercents);
        }

        TEST_METHOD (SpacedGridDerivesThroughFacingEdges)
        {
            const auto grid = GridInfo(2, 2, { 5000, 5000 }, { 5000, 5000 }, { { 0, 1 }, { 2, 3 } }, 16);
            const auto zones = ZonesOf(grid, m_workArea, 16);

            // With spacing 16 the row boundary is the low line at 400-8=392;
            // the facing tops rest 16 above at 408.
            const auto moved = GridTracks::MoveHorizontalBoundary(zones, 392, 50, 16, 0);
            Assert::IsTrue(moved.has_value());

            const auto result = LayoutConfigurator::DeriveGridTrackPercents(moved.value(), grid, m_workArea, 16);
            Assert::IsTrue(result.has_value());
            Assert::AreEqual(std::vector<int>({ 5625, 4375 }), result->rowsPercents);
            Assert::AreEqual(std::vector<int>({ 5000, 5000 }), result->columnsPercents);
            AssertInvariant(result.value(), 2, 2);
        }

        TEST_METHOD (OddSpacingUsesTheEffectiveInternalGap)
        {
            const auto grid = GridInfo(2, 2, { 5000, 5000 }, { 5000, 5000 }, { { 0, 1 }, { 2, 3 } }, 15);
            const auto zones = ZonesOf(grid, m_workArea, 15);

            // CalculateGridZones splits spacing with integer halves. For 15,
            // the facing edges are 393 and 407, an effective 14-pixel gap.
            const auto moved = GridTracks::MoveHorizontalBoundary(zones, 393, 50, 14, 0);
            Assert::IsTrue(moved.has_value());

            const auto result = LayoutConfigurator::DeriveGridTrackPercents(moved.value(), grid, m_workArea, 15);
            Assert::IsTrue(result.has_value());
            Assert::AreEqual(std::vector<int>({ 5625, 4375 }), result->rowsPercents);
            Assert::AreEqual(std::vector<int>({ 5000, 5000 }), result->columnsPercents);
            AssertInvariant(result.value(), 2, 2);
        }

        TEST_METHOD (SpanningZoneKeepsBrokenCutConsistent)
        {
            // Zone 1 spans both rows of the middle column, breaking the row
            // boundary into two separated visible segments.
            const auto grid = GridInfo(2, 3, { 5000, 5000 }, { 3333, 3334, 3333 }, { { 0, 1, 2 }, { 3, 1, 4 } });
            const auto zones = ZonesOf(grid, m_workArea, 0);

            const auto moved = GridTracks::MoveHorizontalBoundary(zones, 400, 80, 0, 0);
            Assert::IsTrue(moved.has_value());

            const auto result = LayoutConfigurator::DeriveGridTrackPercents(moved.value(), grid, m_workArea, 0);
            Assert::IsTrue(result.has_value());
            Assert::AreEqual(std::vector<int>({ 6000, 4000 }), result->rowsPercents);
            Assert::AreEqual(std::vector<int>({ 3333, 3334, 3333 }), result->columnsPercents);
            AssertInvariant(result.value(), 2, 3);
        }

        TEST_METHOD (FullyHiddenBoundaryKeepsOriginalCumulative)
        {
            // Both zones span every row, so the internal row boundary has no
            // visible segment at all and keeps its stored percent.
            const auto grid = GridInfo(2, 2, { 3000, 7000 }, { 5000, 5000 }, { { 0, 1 }, { 0, 1 } });
            const auto zones = ZonesOf(grid, m_workArea, 0);

            const auto moved = GridTracks::MoveVerticalBoundary(zones, 500, 60, 0, 0);
            Assert::IsTrue(moved.has_value());

            const auto result = LayoutConfigurator::DeriveGridTrackPercents(moved.value(), grid, m_workArea, 0);
            Assert::IsTrue(result.has_value());
            Assert::AreEqual(std::vector<int>({ 3000, 7000 }), result->rowsPercents);
            Assert::AreEqual(std::vector<int>({ 5600, 4400 }), result->columnsPercents);
        }

        TEST_METHOD (DerivedBoundaryRoundsToReproducedPixel)
        {
            const FancyZonesUtils::Rect oddArea{ RECT{ 0, 0, 1000, 801 } };
            const auto grid = GridInfo(3, 1, { 2000, 5000, 3000 }, { 10000 }, { { 0 }, { 1 }, { 2 } });
            const auto zones = ZonesOf(grid, oddArea, 0);

            // Boundaries sit at 160 (2000*801/10000) and 560 (7000*801/10000).
            // The first moves to y=200; the unmoved second boundary keeps its
            // original cumulative percent even though 6992 would reproduce the
            // same pixel line.
            const auto moved = GridTracks::MoveHorizontalBoundary(zones, 160, 40, 0, 0);
            Assert::IsTrue(moved.has_value());

            const auto result = LayoutConfigurator::DeriveGridTrackPercents(moved.value(), grid, oddArea, 0);
            Assert::IsTrue(result.has_value());
            Assert::AreEqual(std::vector<int>({ 2497, 4503, 3000 }), result->rowsPercents);
            Assert::AreEqual(std::vector<int>({ 10000 }), result->columnsPercents);
            AssertInvariant(result.value(), 3, 1);
        }

        TEST_METHOD (UnmovedMapKeepsOriginalPercents)
        {
            const auto grid = GridInfo(2, 3, { 5000, 5000 }, { 3333, 3334, 3333 }, { { 0, 1, 2 }, { 3, 4, 5 } });
            const auto zones = ZonesOf(grid, m_workArea, 0);

            const auto result = LayoutConfigurator::DeriveGridTrackPercents(zones, grid, m_workArea, 0);
            Assert::IsTrue(result.has_value());
            Assert::AreEqual(std::vector<int>({ 5000, 5000 }), result->rowsPercents);
            Assert::AreEqual(std::vector<int>({ 3333, 3334, 3333 }), result->columnsPercents);
        }

        TEST_METHOD (DerivedPercentsReproduceTheMovedZones)
        {
            const auto grid = GridInfo(2, 3, { 5000, 5000 }, { 3333, 3334, 3333 }, { { 0, 1, 2 }, { 3, 1, 4 } });
            const auto zones = ZonesOf(grid, m_workArea, 0);
            const auto moved = GridTracks::MoveCornerBoundaries(zones, 400, 80, 333, 50, 0, 0);
            Assert::IsTrue(moved.has_value());

            const auto result = LayoutConfigurator::DeriveGridTrackPercents(moved.value(), grid, m_workArea, 0);
            Assert::IsTrue(result.has_value());

            auto updated = grid;
            updated.m_rowsPercents = result->rowsPercents;
            updated.m_columnsPercents = result->columnsPercents;
            const auto reproduced = ZonesOf(updated, m_workArea, 0);

            Assert::AreEqual(moved->size(), reproduced.size());
            for (const auto& [id, zone] : moved.value())
            {
                Assert::IsTrue(reproduced.contains(id));
                CustomAssert::AreEqual(zone.GetZoneRect(), reproduced.at(id).GetZoneRect());
            }
        }

        TEST_METHOD (EmptyZonesMapIsRejected)
        {
            const auto grid = GridInfo(2, 2, { 5000, 5000 }, { 5000, 5000 }, { { 0, 1 }, { 2, 3 } });
            Assert::IsFalse(LayoutConfigurator::DeriveGridTrackPercents(ZonesMap{}, grid, m_workArea, 0).has_value());
        }

        TEST_METHOD (MissingZoneIsRejected)
        {
            const auto grid = GridInfo(2, 2, { 5000, 5000 }, { 5000, 5000 }, { { 0, 1 }, { 2, 3 } });
            auto zones = ZonesOf(grid, m_workArea, 0);
            zones.erase(3);
            Assert::IsFalse(LayoutConfigurator::DeriveGridTrackPercents(zones, grid, m_workArea, 0).has_value());
        }

        TEST_METHOD (UnknownZoneIsRejected)
        {
            const auto grid = GridInfo(2, 2, { 5000, 5000 }, { 5000, 5000 }, { { 0, 1 }, { 2, 3 } });
            auto zones = ZonesOf(grid, m_workArea, 0);
            zones.emplace(9, Zone(RECT{ 0, 0, 10, 10 }, 9));
            Assert::IsFalse(LayoutConfigurator::DeriveGridTrackPercents(zones, grid, m_workArea, 0).has_value());
        }

        TEST_METHOD (DisagreeingBoundarySegmentsAreRejected)
        {
            const auto grid = GridInfo(2, 1, { 5000, 5000 }, { 10000 }, { { 0 }, { 1 } });

            ZonesMap zones;
            zones.emplace(0, Zone(RECT{ 0, 0, 1000, 400 }, 0));
            zones.emplace(1, Zone(RECT{ 0, 410, 1000, 800 }, 1)); // top does not face the boundary at 400

            Assert::IsFalse(LayoutConfigurator::DeriveGridTrackPercents(zones, grid, m_workArea, 0).has_value());
        }

        TEST_METHOD (WrongCellChildMapShapeIsRejected)
        {
            auto grid = GridInfo(2, 2, { 5000, 5000 }, { 5000, 5000 }, { { 0, 1 }, { 2, 3 } });
            const auto zones = ZonesOf(grid, m_workArea, 0);

            grid.m_cellChildMap = { { 0, 1 } }; // fewer rows than declared
            Assert::IsFalse(LayoutConfigurator::DeriveGridTrackPercents(zones, grid, m_workArea, 0).has_value());

            grid = GridInfo(2, 2, { 5000, 5000 }, { 5000, 5000 }, { { 0, 1 }, { 2, 3 } });
            grid.m_cellChildMap[1] = { 2 }; // fewer columns than declared
            Assert::IsFalse(LayoutConfigurator::DeriveGridTrackPercents(zones, grid, m_workArea, 0).has_value());
        }

        TEST_METHOD (NegativeCellIdIsRejected)
        {
            const auto grid = GridInfo(1, 2, { 10000 }, { 5000, 5000 }, { { 0, -1 } });

            ZonesMap zones;
            zones.emplace(0, Zone(RECT{ 0, 0, 500, 800 }, 0));
            zones.emplace(1, Zone(RECT{ 500, 0, 1000, 800 }, 1));

            Assert::IsFalse(LayoutConfigurator::DeriveGridTrackPercents(zones, grid, m_workArea, 0).has_value());
        }

        TEST_METHOD (NonRectangularZoneIsRejected)
        {
            // Zone 0 covers (0,0), (1,0) and (1,1): an L-shape no grid topology
            // can reproduce.
            const auto grid = GridInfo(2, 2, { 5000, 5000 }, { 5000, 5000 }, { { 0, 1 }, { 0, 0 } });

            ZonesMap zones;
            zones.emplace(0, Zone(RECT{ 0, 0, 500, 800 }, 0));
            zones.emplace(1, Zone(RECT{ 500, 0, 1000, 400 }, 1));

            Assert::IsFalse(LayoutConfigurator::DeriveGridTrackPercents(zones, grid, m_workArea, 0).has_value());
        }

        TEST_METHOD (NonPositiveOriginalPercentIsRejected)
        {
            const auto grid = GridInfo(2, 1, { 10000, 0 }, { 10000 }, { { 0 }, { 0 } });

            ZonesMap zones;
            zones.emplace(0, Zone(RECT{ 0, 0, 1000, 800 }, 0));

            Assert::IsFalse(LayoutConfigurator::DeriveGridTrackPercents(zones, grid, m_workArea, 0).has_value());
        }

        TEST_METHOD (OriginalPercentSumMismatchIsRejected)
        {
            const auto grid = GridInfo(2, 1, { 6000, 6000 }, { 10000 }, { { 0 }, { 0 } });

            ZonesMap zones;
            zones.emplace(0, Zone(RECT{ 0, 0, 1000, 800 }, 0));

            Assert::IsFalse(LayoutConfigurator::DeriveGridTrackPercents(zones, grid, m_workArea, 0).has_value());
        }

        TEST_METHOD (EmptyWorkAreaIsRejected)
        {
            const auto grid = GridInfo(2, 2, { 5000, 5000 }, { 5000, 5000 }, { { 0, 1 }, { 2, 3 } });
            const auto zones = ZonesOf(grid, m_workArea, 0);
            Assert::IsFalse(LayoutConfigurator::DeriveGridTrackPercents(zones, grid, FancyZonesUtils::Rect{ RECT{ 0, 0, 0, 0 } }, 0).has_value());
        }

        TEST_METHOD (HiddenBoundaryBehindMovedLineIsRejected)
        {
            // Zone 1 spans rows 1-2 and hides the second row boundary; the
            // visible first boundary moved past the hidden one's original
            // position, so no positive percent vector can reproduce the map.
            const auto grid = GridInfo(3, 1, { 3333, 3334, 3333 }, { 10000 }, { { 0 }, { 1 }, { 1 } });

            ZonesMap zones;
            zones.emplace(0, Zone(RECT{ 0, 0, 1000, 700 }, 0));
            zones.emplace(1, Zone(RECT{ 0, 700, 1000, 800 }, 1));

            Assert::IsFalse(LayoutConfigurator::DeriveGridTrackPercents(zones, grid, m_workArea, 0).has_value());
        }
    };
}
