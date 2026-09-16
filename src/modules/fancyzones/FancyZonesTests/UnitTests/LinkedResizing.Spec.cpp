#include "pch.h"
#include "Util.h"

#include <filesystem>

#include <FancyZonesLib/LinkedResizing.h>
#include <FancyZonesLib/ModuleConstants.h>
#include <FancyZonesLib/Settings.h>
#include <common/SettingsAPI/settings_helpers.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace FancyZonesUnitTests
{
    TEST_CLASS (LinkedResizingGeometryUnitTest)
    {
        // Configured spacing of 16 + border slack, as WindowLinkedResize computes it.
        static constexpr int kMaxEdgeGap = 16 + LinkedResizing::kBorderSlack;

        TEST_METHOD (RightEdgeResizePullsNeighborLeftEdge)
        {
            const RECT draggedBefore{ 0, 0, 100, 100 };
            const RECT draggedAfter{ 0, 0, 150, 100 };
            const RECT peerBefore{ 116, 0, 216, 100 };

            const auto target = LinkedResizing::ComputeLinkedPeerRect(draggedBefore, draggedAfter, peerBefore, false, kMaxEdgeGap);

            Assert::IsTrue(target.has_value());
            CustomAssert::AreEqual(RECT{ 166, 0, 216, 100 }, target.value());
        }

        TEST_METHOD (LeftEdgeResizePullsNeighborRightEdge)
        {
            const RECT draggedBefore{ 116, 0, 216, 100 };
            const RECT draggedAfter{ 66, 0, 216, 100 };
            const RECT peerBefore{ 0, 0, 100, 100 };

            const auto target = LinkedResizing::ComputeLinkedPeerRect(draggedBefore, draggedAfter, peerBefore, false, kMaxEdgeGap);

            Assert::IsTrue(target.has_value());
            CustomAssert::AreEqual(RECT{ 0, 0, 50, 100 }, target.value());
        }

        TEST_METHOD (BottomEdgeResizePullsNeighborTopEdge)
        {
            const RECT draggedBefore{ 0, 0, 100, 100 };
            const RECT draggedAfter{ 0, 0, 100, 150 };
            const RECT peerBefore{ 0, 116, 100, 216 };

            const auto target = LinkedResizing::ComputeLinkedPeerRect(draggedBefore, draggedAfter, peerBefore, false, kMaxEdgeGap);

            Assert::IsTrue(target.has_value());
            CustomAssert::AreEqual(RECT{ 0, 166, 100, 216 }, target.value());
        }

        TEST_METHOD (TopEdgeResizePullsNeighborBottomEdge)
        {
            const RECT draggedBefore{ 0, 116, 100, 216 };
            const RECT draggedAfter{ 0, 66, 100, 216 };
            const RECT peerBefore{ 0, 0, 100, 100 };

            const auto target = LinkedResizing::ComputeLinkedPeerRect(draggedBefore, draggedAfter, peerBefore, false, kMaxEdgeGap);

            Assert::IsTrue(target.has_value());
            CustomAssert::AreEqual(RECT{ 0, 0, 100, 50 }, target.value());
        }

        TEST_METHOD (RightEdgeResizeLinksAllBorderingNeighbors)
        {
            const RECT draggedBefore{ 0, 0, 100, 100 };
            const RECT draggedAfter{ 0, 0, 150, 100 };
            const RECT peer1Before{ 116, 0, 216, 60 };
            const RECT peer2Before{ 116, 40, 216, 120 };

            const auto target1 = LinkedResizing::ComputeLinkedPeerRect(draggedBefore, draggedAfter, peer1Before, false, kMaxEdgeGap);
            const auto target2 = LinkedResizing::ComputeLinkedPeerRect(draggedBefore, draggedAfter, peer2Before, false, kMaxEdgeGap);

            Assert::IsTrue(target1.has_value());
            Assert::IsTrue(target2.has_value());
            CustomAssert::AreEqual(RECT{ 166, 0, 216, 60 }, target1.value());
            CustomAssert::AreEqual(RECT{ 166, 40, 216, 120 }, target2.value());
        }

        TEST_METHOD (CornerResizeLinksRightBottomAndDiagonalNeighbors)
        {
            const RECT draggedBefore{ 0, 0, 100, 100 };
            const RECT draggedAfter{ 0, 0, 150, 150 };
            const RECT rightPeer{ 116, 0, 216, 100 };
            const RECT bottomPeer{ 0, 116, 100, 216 };
            const RECT diagonalPeer{ 116, 116, 216, 216 };

            const auto rightTarget = LinkedResizing::ComputeLinkedPeerRect(draggedBefore, draggedAfter, rightPeer, false, kMaxEdgeGap);
            const auto bottomTarget = LinkedResizing::ComputeLinkedPeerRect(draggedBefore, draggedAfter, bottomPeer, false, kMaxEdgeGap);
            const auto diagonalTarget = LinkedResizing::ComputeLinkedPeerRect(draggedBefore, draggedAfter, diagonalPeer, false, kMaxEdgeGap);

            Assert::IsTrue(rightTarget.has_value());
            Assert::IsTrue(bottomTarget.has_value());
            Assert::IsTrue(diagonalTarget.has_value());
            CustomAssert::AreEqual(RECT{ 166, 0, 216, 100 }, rightTarget.value());
            CustomAssert::AreEqual(RECT{ 0, 166, 100, 216 }, bottomTarget.value());
            CustomAssert::AreEqual(RECT{ 166, 166, 216, 216 }, diagonalTarget.value());
        }

        TEST_METHOD (SameZoneStackedWindowStaysAligned)
        {
            const RECT draggedBefore{ 0, 0, 100, 100 };
            const RECT draggedAfter{ 10, 10, 140, 130 };
            const RECT peerBefore{ 0, 0, 100, 100 };

            const auto target = LinkedResizing::ComputeLinkedPeerRect(draggedBefore, draggedAfter, peerBefore, true, kMaxEdgeGap);

            Assert::IsTrue(target.has_value());
            CustomAssert::AreEqual(draggedAfter, target.value());
        }

        TEST_METHOD (GapBetweenNeighborsIsPreserved)
        {
            const RECT draggedBefore{ 0, 0, 100, 100 };
            const RECT draggedAfter{ 0, 0, 140, 100 };
            const RECT peerBefore{ 116, 0, 216, 100 };

            const auto target = LinkedResizing::ComputeLinkedPeerRect(draggedBefore, draggedAfter, peerBefore, false, kMaxEdgeGap);

            Assert::IsTrue(target.has_value());
            Assert::AreEqual<LONG>(peerBefore.left - draggedBefore.right, target->left - draggedAfter.right);
        }

        TEST_METHOD (ZeroGapNeighborsAreLinked)
        {
            const RECT draggedBefore{ 0, 0, 100, 100 };
            const RECT draggedAfter{ 0, 0, 130, 100 };
            const RECT peerBefore{ 100, 0, 200, 100 };

            const auto target = LinkedResizing::ComputeLinkedPeerRect(draggedBefore, draggedAfter, peerBefore, false, kMaxEdgeGap);

            Assert::IsTrue(target.has_value());
            CustomAssert::AreEqual(RECT{ 130, 0, 200, 100 }, target.value());
        }

        TEST_METHOD (SlightlyOverlappingBordersAreLinked)
        {
            // Window rects include invisible borders, so touching frames report a small
            // negative gap between their window rects.
            const RECT draggedBefore{ 0, 0, 100, 100 };
            const RECT draggedAfter{ 0, 0, 130, 100 };
            const RECT peerBefore{ 90, 0, 200, 100 };

            const auto target = LinkedResizing::ComputeLinkedPeerRect(draggedBefore, draggedAfter, peerBefore, false, kMaxEdgeGap);

            Assert::IsTrue(target.has_value());
            CustomAssert::AreEqual(RECT{ 120, 0, 200, 100 }, target.value());
        }

        TEST_METHOD (LaterSessionLinksRectsThatNoLongerMatchZones)
        {
            // First session moved the shared border: both windows no longer match their
            // original zone rects but still directly border each other.
            const RECT firstDraggedBefore{ 0, 0, 100, 100 };
            const RECT firstDraggedAfter{ 0, 0, 150, 100 };
            const RECT firstPeerBefore{ 116, 0, 216, 100 };

            const auto firstTarget = LinkedResizing::ComputeLinkedPeerRect(firstDraggedBefore, firstDraggedAfter, firstPeerBefore, false, kMaxEdgeGap);
            Assert::IsTrue(firstTarget.has_value());
            Assert::AreEqual<LONG>(16, firstTarget->left - firstDraggedAfter.right);

            // Second session starts from the rects produced by the first session.
            const RECT secondDraggedAfter{ 0, 0, 190, 100 };
            const auto secondTarget = LinkedResizing::ComputeLinkedPeerRect(firstDraggedAfter, secondDraggedAfter, firstTarget.value(), false, kMaxEdgeGap);

            Assert::IsTrue(secondTarget.has_value());
            CustomAssert::AreEqual(RECT{ 206, 0, 216, 100 }, secondTarget.value());
            Assert::AreEqual<LONG>(16, secondTarget->left - secondDraggedAfter.right);
        }

        TEST_METHOD (DetachedNeighborIsNotLinked)
        {
            const RECT draggedBefore{ 0, 0, 100, 100 };
            const RECT draggedAfter{ 0, 0, 150, 100 };
            const RECT peerBefore{ 200, 0, 300, 100 }; // gap 100 > maxEdgeGap

            const auto target = LinkedResizing::ComputeLinkedPeerRect(draggedBefore, draggedAfter, peerBefore, false, kMaxEdgeGap);

            Assert::IsFalse(target.has_value());
        }

        TEST_METHOD (DeeplyOverlappingNeighborIsNotLinked)
        {
            const RECT draggedBefore{ 0, 0, 100, 100 };
            const RECT draggedAfter{ 0, 0, 150, 100 };
            const RECT peerBefore{ 0, 0, 200, 100 }; // rects overlap far beyond border slack

            const auto target = LinkedResizing::ComputeLinkedPeerRect(draggedBefore, draggedAfter, peerBefore, false, kMaxEdgeGap);

            Assert::IsFalse(target.has_value());
        }

        TEST_METHOD (NeighborWithoutFacingEdgeIsNotLinked)
        {
            const RECT draggedBefore{ 0, 0, 100, 100 };
            const RECT draggedAfter{ 0, 0, 150, 100 };
            const RECT peerBefore{ 116, 200, 216, 300 }; // bordering on X, far away on Y

            const auto target = LinkedResizing::ComputeLinkedPeerRect(draggedBefore, draggedAfter, peerBefore, false, kMaxEdgeGap);

            Assert::IsFalse(target.has_value());
        }

        TEST_METHOD (NeighborOnUnmovedSideStaysUnchanged)
        {
            const RECT draggedBefore{ 116, 0, 216, 100 };
            const RECT draggedAfter{ 116, 0, 266, 100 }; // only the right edge moved
            const RECT peerBefore{ 0, 0, 100, 100 };     // neighbor to the left

            const auto target = LinkedResizing::ComputeLinkedPeerRect(draggedBefore, draggedAfter, peerBefore, false, kMaxEdgeGap);

            // The peer is adjacent to the unmoved left edge, so it keeps its rect.
            Assert::IsTrue(target.has_value());
            CustomAssert::AreEqual(peerBefore, target.value());
        }

        TEST_METHOD (UnmovedWindowLeavesNeighborsUnchanged)
        {
            const RECT draggedRect{ 0, 0, 100, 100 };
            const RECT peerBefore{ 116, 0, 216, 100 };

            const auto target = LinkedResizing::ComputeLinkedPeerRect(draggedRect, draggedRect, peerBefore, false, kMaxEdgeGap);

            Assert::IsTrue(target.has_value());
            CustomAssert::AreEqual(peerBefore, target.value());
        }

        TEST_METHOD (RevertedEdgeRestoresNeighborRect)
        {
            // Dragging an edge out and back restores the linked peer to its start rect.
            const RECT draggedBefore{ 0, 0, 100, 100 };
            const RECT draggedAfter{ 0, 0, 150, 100 };
            const RECT peerBefore{ 116, 0, 216, 100 };

            const auto moved = LinkedResizing::ComputeLinkedPeerRect(draggedBefore, draggedAfter, peerBefore, false, kMaxEdgeGap);
            Assert::IsTrue(moved.has_value());
            Assert::AreNotEqual(peerBefore.left, moved->left);

            const auto restored = LinkedResizing::ComputeLinkedPeerRect(draggedBefore, draggedBefore, peerBefore, false, kMaxEdgeGap);
            Assert::IsTrue(restored.has_value());
            CustomAssert::AreEqual(peerBefore, restored.value());
        }

        TEST_METHOD (InvalidTargetWidthIsRejected)
        {
            const RECT draggedBefore{ 0, 0, 100, 100 };
            const RECT draggedAfter{ 0, 0, 400, 100 }; // grows over the neighbor
            const RECT peerBefore{ 116, 0, 200, 100 };

            const auto target = LinkedResizing::ComputeLinkedPeerRect(draggedBefore, draggedAfter, peerBefore, false, kMaxEdgeGap);

            Assert::IsFalse(target.has_value());
        }

        TEST_METHOD (InvalidTargetHeightIsRejected)
        {
            const RECT draggedBefore{ 0, 0, 100, 100 };
            const RECT draggedAfter{ 0, 0, 100, 400 }; // grows over the neighbor
            const RECT peerBefore{ 0, 116, 100, 200 };

            const auto target = LinkedResizing::ComputeLinkedPeerRect(draggedBefore, draggedAfter, peerBefore, false, kMaxEdgeGap);

            Assert::IsFalse(target.has_value());
        }

        TEST_METHOD (EdgeDeltasReportOnlyMovedEdges)
        {
            const RECT before{ 10, 20, 110, 220 };
            const RECT after{ 10, 25, 160, 220 };

            const auto deltas = LinkedResizing::ComputeEdgeDeltas(before, after);

            Assert::AreEqual<LONG>(0, deltas.left);
            Assert::AreEqual<LONG>(5, deltas.top);
            Assert::AreEqual<LONG>(50, deltas.right);
            Assert::AreEqual<LONG>(0, deltas.bottom);
            Assert::IsTrue(deltas.AnyMoved());
        }
    };

    TEST_CLASS (LinkedResizingSettingsUnitTest)
    {
        TEST_METHOD_INITIALIZE(Init)
        {
            FancyZonesSettings::instance().SetSettings(Settings{});
        }

        TEST_METHOD_CLEANUP(Cleanup)
        {
            std::filesystem::remove(FancyZonesSettings::GetSettingsFileName());
            FancyZonesSettings::instance().SetSettings(Settings{});
        }

        TEST_METHOD (DefaultIsEnabled)
        {
            Assert::IsTrue(Settings{}.linkedResizing);
        }

        TEST_METHOD (ParsesDisabledValue)
        {
            PowerToysSettings::PowerToyValues values(NonLocalizable::ModuleKey, NonLocalizable::ModuleKey);
            values.add_property(L"fancyzones_linkedResizing", false);
            json::to_file(FancyZonesSettings::GetSettingsFileName(), values.get_raw_json());

            FancyZonesSettings::instance().LoadSettings();

            Assert::IsFalse(FancyZonesSettings::settings().linkedResizing);
        }

        TEST_METHOD (ParsesEnabledValue)
        {
            PowerToysSettings::PowerToyValues values(NonLocalizable::ModuleKey, NonLocalizable::ModuleKey);
            values.add_property(L"fancyzones_linkedResizing", true);
            json::to_file(FancyZonesSettings::GetSettingsFileName(), values.get_raw_json());

            FancyZonesSettings::instance().LoadSettings();

            Assert::IsTrue(FancyZonesSettings::settings().linkedResizing);
        }

        TEST_METHOD (MissingValueKeepsEnabledDefault)
        {
            PowerToysSettings::PowerToyValues values(NonLocalizable::ModuleKey, NonLocalizable::ModuleKey);
            values.add_property(L"fancyzones_shiftDrag", true);
            json::to_file(FancyZonesSettings::GetSettingsFileName(), values.get_raw_json());

            FancyZonesSettings::instance().LoadSettings();

            Assert::IsTrue(FancyZonesSettings::settings().linkedResizing);
        }
    };
}
