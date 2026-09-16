#include "pch.h"
#include "Util.h"

#include <FancyZonesLib/GridTracks.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace FancyZonesUnitTests
{
    TEST_CLASS (GridTracksUnitTest)
    {
        static constexpr LONG kGap = 16;
        static constexpr LONG kTrackY = 242;  // horizontal boundary line
        static constexpr LONG kTrackX = 288;  // vertical boundary line

        // "Broken-cut" grid: two rows where the middle column zone spans the row
        // boundary, leaving two separated visible boundary segments, plus a
        // vertical boundary crossed by that same spanning zone.
        //
        //        288   304        596   612
        //   +----------+ +----------+ +----------+
        //   |    0     | |          | |    2     |
        //   +----------+ |    1     | +----------+  <- track at y=242
        //   |    3     | |  (spans) | |    4     |
        //   +----------+ +----------+ +----------+
        //  boundary: bottoms at 242, facing tops at 242 + 16 = 258
        static ZonesMap BrokenCutGrid()
        {
            ZonesMap zones;
            zones.emplace(0, Zone(RECT{ 16, 16, 288, 242 }, 0));
            zones.emplace(1, Zone(RECT{ 304, 16, 596, 484 }, 1));
            zones.emplace(2, Zone(RECT{ 612, 16, 884, 242 }, 2));
            zones.emplace(3, Zone(RECT{ 16, 258, 288, 484 }, 3));
            zones.emplace(4, Zone(RECT{ 612, 258, 884, 484 }, 4));
            return zones;
        }

        TEST_METHOD (HorizontalBoundaryMovesEverySeparatedSegment)
        {
            const auto moved = GridTracks::MoveHorizontalBoundary(BrokenCutGrid(), kTrackY, 50, kGap, 1);

            Assert::IsTrue(moved.has_value());
            CustomAssert::AreEqual(RECT{ 16, 16, 288, 292 }, moved->at(0).GetZoneRect());
            CustomAssert::AreEqual(RECT{ 304, 16, 596, 484 }, moved->at(1).GetZoneRect());
            CustomAssert::AreEqual(RECT{ 612, 16, 884, 292 }, moved->at(2).GetZoneRect());
            CustomAssert::AreEqual(RECT{ 16, 308, 288, 484 }, moved->at(3).GetZoneRect());
            CustomAssert::AreEqual(RECT{ 612, 308, 884, 484 }, moved->at(4).GetZoneRect());
        }

        TEST_METHOD (HorizontalBoundaryMovePreservesGap)
        {
            const auto moved = GridTracks::MoveHorizontalBoundary(BrokenCutGrid(), kTrackY, 50, kGap, 1);

            Assert::IsTrue(moved.has_value());
            Assert::AreEqual<LONG>(kGap, moved->at(3).GetZoneRect().top - moved->at(0).GetZoneRect().bottom);
            Assert::AreEqual<LONG>(kGap, moved->at(4).GetZoneRect().top - moved->at(2).GetZoneRect().bottom);
        }

        TEST_METHOD (BoundaryMovePreservesZoneIds)
        {
            const ZonesMap zones = BrokenCutGrid();
            const auto moved = GridTracks::MoveHorizontalBoundary(zones, kTrackY, 50, kGap, 1);

            Assert::IsTrue(moved.has_value());
            Assert::AreEqual(zones.size(), moved->size());
            for (const auto& [id, zone] : zones)
            {
                Assert::IsTrue(moved->contains(id));
                Assert::AreEqual(id, moved->at(id).Id());
            }
        }

        TEST_METHOD (SpanningZoneRemainsUnchanged)
        {
            const auto moved = GridTracks::MoveHorizontalBoundary(BrokenCutGrid(), kTrackY, -60, kGap, 1);

            Assert::IsTrue(moved.has_value());
            CustomAssert::AreEqual(RECT{ 304, 16, 596, 484 }, moved->at(1).GetZoneRect());
            Assert::AreEqual<ZoneIndex>(1, moved->at(1).Id());
        }

        TEST_METHOD (VerticalBoundaryMovesEverySegment)
        {
            // Boundary at x=288: right edges of zones 0 and 3 face the left edge
            // of the spanning zone 1 at 304; zones 2 and 4 are untouched.
            const auto moved = GridTracks::MoveVerticalBoundary(BrokenCutGrid(), kTrackX, 40, kGap, 1);

            Assert::IsTrue(moved.has_value());
            CustomAssert::AreEqual(RECT{ 16, 16, 328, 242 }, moved->at(0).GetZoneRect());
            CustomAssert::AreEqual(RECT{ 344, 16, 596, 484 }, moved->at(1).GetZoneRect());
            CustomAssert::AreEqual(RECT{ 612, 16, 884, 242 }, moved->at(2).GetZoneRect());
            CustomAssert::AreEqual(RECT{ 16, 258, 328, 484 }, moved->at(3).GetZoneRect());
            CustomAssert::AreEqual(RECT{ 612, 258, 884, 484 }, moved->at(4).GetZoneRect());
            Assert::AreEqual<LONG>(kGap, moved->at(1).GetZoneRect().left - moved->at(0).GetZoneRect().right);
        }

        TEST_METHOD (CornerMoveComposesHorizontalAndVertical)
        {
            const auto moved = GridTracks::MoveCornerBoundaries(BrokenCutGrid(), kTrackY, 50, kTrackX, 40, kGap, 1);

            Assert::IsTrue(moved.has_value());
            CustomAssert::AreEqual(RECT{ 16, 16, 328, 292 }, moved->at(0).GetZoneRect());   // right + bottom
            CustomAssert::AreEqual(RECT{ 344, 16, 596, 484 }, moved->at(1).GetZoneRect());  // left only
            CustomAssert::AreEqual(RECT{ 612, 16, 884, 292 }, moved->at(2).GetZoneRect());  // bottom only
            CustomAssert::AreEqual(RECT{ 16, 308, 328, 484 }, moved->at(3).GetZoneRect());  // top + right
            CustomAssert::AreEqual(RECT{ 612, 308, 884, 484 }, moved->at(4).GetZoneRect()); // top only
        }

        TEST_METHOD (ZeroDeltaKeepsZoneRects)
        {
            const ZonesMap zones = BrokenCutGrid();
            const auto moved = GridTracks::MoveHorizontalBoundary(zones, kTrackY, 0, kGap, 1);

            Assert::IsTrue(moved.has_value());
            for (const auto& [id, zone] : zones)
            {
                CustomAssert::AreEqual(zone.GetZoneRect(), moved->at(id).GetZoneRect());
            }
        }

        TEST_METHOD (ZeroGapBoundaryMoves)
        {
            ZonesMap zones;
            zones.emplace(0, Zone(RECT{ 0, 0, 100, 100 }, 0));
            zones.emplace(1, Zone(RECT{ 0, 100, 100, 200 }, 1));

            const auto moved = GridTracks::MoveHorizontalBoundary(zones, 100, 20, 0, 1);

            Assert::IsTrue(moved.has_value());
            CustomAssert::AreEqual(RECT{ 0, 0, 100, 120 }, moved->at(0).GetZoneRect());
            CustomAssert::AreEqual(RECT{ 0, 120, 100, 200 }, moved->at(1).GetZoneRect());
        }

        TEST_METHOD (NegativeGapBoundaryMoves)
        {
            // Negative spacing: facing edges overlap by the gap amount.
            ZonesMap zones;
            zones.emplace(0, Zone(RECT{ 0, 0, 100, 116 }, 0));
            zones.emplace(1, Zone(RECT{ 0, 100, 100, 200 }, 1));

            const auto moved = GridTracks::MoveHorizontalBoundary(zones, 116, 10, -16, 1);

            Assert::IsTrue(moved.has_value());
            CustomAssert::AreEqual(RECT{ 0, 0, 100, 126 }, moved->at(0).GetZoneRect());
            CustomAssert::AreEqual(RECT{ 0, 110, 100, 200 }, moved->at(1).GetZoneRect());
            Assert::AreEqual<LONG>(-16, moved->at(1).GetZoneRect().top - moved->at(0).GetZoneRect().bottom);
        }

        TEST_METHOD (OuterBottomEdgeIsRejected)
        {
            // y=484 is the bottom edge of the whole map: zones on one side only.
            Assert::IsFalse(GridTracks::MoveHorizontalBoundary(BrokenCutGrid(), 484, 20, kGap, 1).has_value());
        }

        TEST_METHOD (OuterTopEdgeIsRejected)
        {
            // Tops at 0+16=16 exist (zones 0,1,2) but no zone bottoms at 0.
            Assert::IsFalse(GridTracks::MoveHorizontalBoundary(BrokenCutGrid(), 0, 20, kGap, 1).has_value());
        }

        TEST_METHOD (CoordinateWithoutBoundaryIsRejected)
        {
            Assert::IsFalse(GridTracks::MoveHorizontalBoundary(BrokenCutGrid(), 400, 20, kGap, 1).has_value());
            Assert::IsFalse(GridTracks::MoveVerticalBoundary(BrokenCutGrid(), 400, 20, kGap, 1).has_value());
        }

        TEST_METHOD (EmptyMapIsRejected)
        {
            Assert::IsFalse(GridTracks::MoveHorizontalBoundary(ZonesMap{}, kTrackY, 20, kGap, 1).has_value());
        }

        TEST_METHOD (AmbiguousEdgeInsideGapBandIsRejected)
        {
            ZonesMap zones = BrokenCutGrid();
            zones.emplace(5, Zone(RECT{ 650, 100, 880, 250 }, 5)); // bottom floats inside (242, 258)

            Assert::IsFalse(GridTracks::MoveHorizontalBoundary(zones, kTrackY, 20, kGap, 1).has_value());
        }

        TEST_METHOD (AmbiguousEdgeOnOppositeLineIsRejected)
        {
            ZonesMap zones = BrokenCutGrid();
            zones.emplace(5, Zone(RECT{ 650, 242, 880, 400 }, 5)); // top rests on the upper-side line

            Assert::IsFalse(GridTracks::MoveHorizontalBoundary(zones, kTrackY, 20, kGap, 1).has_value());
        }

        TEST_METHOD (MinimumHeightViolationIsRejected)
        {
            // Moving the boundary down 200px leaves zones 3/4 only 26px tall.
            Assert::IsFalse(GridTracks::MoveHorizontalBoundary(BrokenCutGrid(), kTrackY, 200, kGap, 50).has_value());
        }

        TEST_METHOD (MinimumWidthViolationIsRejected)
        {
            // Moving the x=288 boundary right by 250px leaves zone 1 only 42px wide.
            Assert::IsFalse(GridTracks::MoveVerticalBoundary(BrokenCutGrid(), kTrackX, 250, kGap, 50).has_value());
        }

        TEST_METHOD (NonPositiveTargetIsRejected)
        {
            // Moving the boundary up 300px collapses zones 0/2 (bottom would be -58).
            Assert::IsFalse(GridTracks::MoveHorizontalBoundary(BrokenCutGrid(), kTrackY, -300, kGap, 0).has_value());
        }

        TEST_METHOD (CornerFailureReturnsNoPartialMap)
        {
            // Valid horizontal boundary, but x=700 identifies no vertical boundary.
            Assert::IsFalse(GridTracks::MoveCornerBoundaries(BrokenCutGrid(), kTrackY, 50, 700, 20, kGap, 1).has_value());

            // Valid vertical boundary, but y=400 identifies no horizontal boundary.
            Assert::IsFalse(GridTracks::MoveCornerBoundaries(BrokenCutGrid(), 400, 20, kTrackX, 40, kGap, 1).has_value());
        }
    };
}
