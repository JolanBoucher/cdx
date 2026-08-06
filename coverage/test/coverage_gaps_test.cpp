/**
 * @file coverage_gaps_test.cpp
 * @brief Unit tests for coverage_gaps.cpp (walkSpanToForwardRange,
 *        leadingUncoveredRange, trailingUncoveredRange).
 *
 * These three functions are the entire orientation-math surface of
 * base-pair-precision coverage: everything else in the feature (gam_io.cpp's
 * gap-detection block, cov_projection.cpp's applyBpGaps*) is coordinate
 * bookkeeping around calls into them. They are deliberately free of any
 * Protobuf/vg_io dependency (see coverage_gaps.h's file docstring), so they
 * are tested here directly with plain integers, independent of GAM fixtures.
 *
 * Test node used throughout unless noted otherwise: node_length = 100.
 */

#include <gtest/gtest.h>

#include "../src/coverage_gaps.h"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

// =============================================================================
// walkSpanToForwardRange - forward strand (is_reverse == false).
//
// Forward walk position w sits at forward position `offset + w`, so a
// walk-order range simply shifts by `offset`.
// =============================================================================

TEST(WalkSpanToForwardRangeForwardTest, ZeroOffsetIdentityMapping) {
    const ForwardRange r = walkSpanToForwardRange(100, 0, false, 10, 20);
    EXPECT_EQ(r.start, 10u);
    EXPECT_EQ(r.end, 20u);
}

TEST(WalkSpanToForwardRangeForwardTest, NonZeroOffsetShiftsRange) {
    // A deletion 40 bases into a mapping that itself starts at node offset 30.
    const ForwardRange r = walkSpanToForwardRange(100, 30, false, 40, 45);
    EXPECT_EQ(r.start, 70u);
    EXPECT_EQ(r.end, 75u);
}

TEST(WalkSpanToForwardRangeForwardTest, FullNodeWalkCoversWholeNode) {
    const ForwardRange r = walkSpanToForwardRange(100, 0, false, 0, 100);
    EXPECT_EQ(r.start, 0u);
    EXPECT_EQ(r.end, 100u);
}

// =============================================================================
// walkSpanToForwardRange - reverse strand (is_reverse == true).
//
// Reverse walk position w sits at forward position
// `node_length - offset - 1 - w`, so a walk-order range [a, b) maps to the
// forward range [node_length - offset - b, node_length - offset - a) -
// smaller walk positions land at *larger* forward positions.
// =============================================================================

TEST(WalkSpanToForwardRangeReverseTest, ZeroOffsetFullNodeWalkCoversWholeNodeMirrored) {
    const ForwardRange r = walkSpanToForwardRange(100, 0, true, 0, 100);
    EXPECT_EQ(r.start, 0u);
    EXPECT_EQ(r.end, 100u);
}

TEST(WalkSpanToForwardRangeReverseTest, WalkStartLandsNearNodesForwardEnd) {
    // is_reverse, offset=0: walk position 0 sits at forward position
    // node_length - 0 - 1 - 0 = 99, i.e. the very last base of the node.
    const ForwardRange r = walkSpanToForwardRange(100, 0, true, 0, 1);
    EXPECT_EQ(r.start, 99u);
    EXPECT_EQ(r.end, 100u);
}

TEST(WalkSpanToForwardRangeReverseTest, NonZeroOffsetShiftsMirroredRange) {
    // offset=10: the walk starts 10 bases in from the node's forward end,
    // i.e. at forward position 100-10-1=89, walking down.
    const ForwardRange r = walkSpanToForwardRange(100, 10, true, 0, 50);
    EXPECT_EQ(r.start, 40u);
    EXPECT_EQ(r.end, 90u);
}

TEST(WalkSpanToForwardRangeReverseTest, WalkAndForwardEquivalentCoverSameNodeForDifferentOffsets) {
    // Cross-check the two orientation branches against each other: a
    // forward walk with offset=10 covering [0,50) covers forward [10,60);
    // the *reverse* walk with the same offset covering the same walk-order
    // span covers the complementary high end of the node instead.
    const ForwardRange fwd = walkSpanToForwardRange(100, 10, false, 0, 50);
    const ForwardRange rev = walkSpanToForwardRange(100, 10, true, 0, 50);
    EXPECT_EQ(fwd.start, 10u);
    EXPECT_EQ(fwd.end, 60u);
    EXPECT_EQ(rev.start, 40u);
    EXPECT_EQ(rev.end, 90u);
    // Both cover the same number of bases (the walk-order span length is
    // the same), just mirrored to opposite ends of the node.
    EXPECT_EQ(fwd.end - fwd.start, rev.end - rev.start);
}

// =============================================================================
// walkSpanToForwardRange - defensive clamping on malformed input.
// =============================================================================

TEST(WalkSpanToForwardRangeClampTest, WalkEndPastNodeLengthClampsInsteadOfOverflowing) {
    // Forward, offset=90: a walk_end of 100 would nominally land at forward
    // position 190, which cannot exist on a 100bp node.
    const ForwardRange r = walkSpanToForwardRange(100, 90, false, 0, 100);
    EXPECT_EQ(r.start, 90u);
    EXPECT_EQ(r.end, 100u); // clamped, not 190
}

TEST(WalkSpanToForwardRangeClampTest, ReverseWalkEndPastNodeLengthClampsToZero) {
    // Reverse, offset=90: len-off-walk_end = 100-90-100 = -90, must clamp to 0.
    const ForwardRange r = walkSpanToForwardRange(100, 90, true, 0, 100);
    EXPECT_EQ(r.start, 0u);
    EXPECT_EQ(r.end, 10u);
}

TEST(WalkSpanToForwardRangeClampTest, EmptyWalkSpanReturnsEmptyRange) {
    const ForwardRange r = walkSpanToForwardRange(100, 10, false, 20, 20);
    EXPECT_TRUE(r.empty());
}

// =============================================================================
// leadingUncoveredRange.
// =============================================================================

TEST(LeadingUncoveredRangeTest, ZeroOffsetForwardMeansNoGap) {
    EXPECT_FALSE(leadingUncoveredRange(100, 0, false).has_value());
}

TEST(LeadingUncoveredRangeTest, ZeroOffsetReverseMeansNoGap) {
    EXPECT_FALSE(leadingUncoveredRange(100, 0, true).has_value());
}

TEST(LeadingUncoveredRangeTest, ForwardPositiveOffsetGapsThePrefix) {
    const auto gap = leadingUncoveredRange(100, 30, false);
    ASSERT_TRUE(gap.has_value());
    EXPECT_EQ(gap->start, 0u);
    EXPECT_EQ(gap->end, 30u);
}

TEST(LeadingUncoveredRangeTest, ReversePositiveOffsetGapsTheHighEndInstead) {
    // is_reverse: the walk starts `offset` bases in from the node's forward
    // end, so the never-reached prefix is at the *high* end of the node,
    // not the low end (mirrored relative to the forward case above).
    const auto gap = leadingUncoveredRange(100, 10, true);
    ASSERT_TRUE(gap.has_value());
    EXPECT_EQ(gap->start, 90u);
    EXPECT_EQ(gap->end, 100u);
}

TEST(LeadingUncoveredRangeTest, OffsetClampedToNodeLengthNeverProducesInvertedRange) {
    // Malformed input: offset larger than the node itself.
    const auto gap = leadingUncoveredRange(100, 500, false);
    ASSERT_TRUE(gap.has_value());
    EXPECT_EQ(gap->start, 0u);
    EXPECT_EQ(gap->end, 100u); // clamped to the whole node, not 500
}

// =============================================================================
// trailingUncoveredRange.
// =============================================================================

TEST(TrailingUncoveredRangeTest, FullWalkToNodeEndForwardMeansNoGap) {
    EXPECT_FALSE(trailingUncoveredRange(100, 0, false, 100).has_value());
}

TEST(TrailingUncoveredRangeTest, FullWalkToNodeEndReverseMeansNoGap) {
    EXPECT_FALSE(trailingUncoveredRange(100, 0, true, 100).has_value());
}

TEST(TrailingUncoveredRangeTest, ForwardShortWalkGapsTheSuffix) {
    // Mapping starts at offset 0, but its edits only consume 70 of the
    // node's 100 bases - the alignment ends partway through the node.
    const auto gap = trailingUncoveredRange(100, 0, false, 70);
    ASSERT_TRUE(gap.has_value());
    EXPECT_EQ(gap->start, 70u);
    EXPECT_EQ(gap->end, 100u);
}

TEST(TrailingUncoveredRangeTest, ReverseShortWalkGapsTheLowEndInstead) {
    // is_reverse, offset=10: the walk covers forward [100-10-consumed, 90).
    // With consumed=50 (out of a possible 90 remaining), the walk covers
    // [40, 90); everything below that (towards forward position 0) is the
    // trailing gap, mirrored relative to the forward case above.
    const auto gap = trailingUncoveredRange(100, 10, true, 50);
    ASSERT_TRUE(gap.has_value());
    EXPECT_EQ(gap->start, 0u);
    EXPECT_EQ(gap->end, 40u);
}

TEST(TrailingUncoveredRangeTest, ConsumedPastRemainingNodeLengthClampsInsteadOfInverting) {
    // Malformed input: edits claim to consume more than the node has left
    // past `offset`.
    const auto gap = trailingUncoveredRange(100, 80, false, 1000);
    EXPECT_FALSE(gap.has_value()); // clamped to consuming exactly the remaining 20 bases -> no gap
}

TEST(TrailingUncoveredRangeTest, OffsetAtNodeEndLeavesEmptyRemainingSpan) {
    // offset == node_length: nothing left to consume, no possible gap
    // (walkSpanToForwardRange callers would never reach here with edits in
    // that case, but the function must still behave safely).
    EXPECT_FALSE(trailingUncoveredRange(100, 100, false, 0).has_value());
}

// =============================================================================
// Cross-function consistency: leading + walked span + trailing should
// exactly partition the whole node with no overlap, for both strands.
// =============================================================================

namespace {
    void expectPartitionsWholeNode(
        const cdx::SeqLen node_length,
        const cdx::SeqLen offset,
        const bool is_reverse,
        const cdx::SeqLen total_from_consumed
    ) {
        const ForwardRange walked = walkSpanToForwardRange(node_length, offset, is_reverse, 0, total_from_consumed);
        const auto leading = leadingUncoveredRange(node_length, offset, is_reverse);
        const auto trailing = trailingUncoveredRange(node_length, offset, is_reverse, total_from_consumed);

        const cdx::SeqLen leading_start = leading ? leading->start : walked.start;
        const cdx::SeqLen leading_end = leading ? leading->end : walked.start;
        const cdx::SeqLen trailing_start = trailing ? trailing->start : walked.end;
        const cdx::SeqLen trailing_end = trailing ? trailing->end : walked.end;

        // No gaps at all: walked span alone must cover [0, node_length).
        if (!leading && !trailing) {
            EXPECT_EQ(walked.start, 0u);
            EXPECT_EQ(walked.end, node_length);
            return;
        }

        // Otherwise, the three pieces must exactly tile [0, node_length)
        // with no overlap and no hole, regardless of which side is
        // "leading" vs "trailing" (that flips with strand).
        std::vector<std::pair<cdx::SeqLen, cdx::SeqLen> > pieces{
            {leading_start, leading_end}, {walked.start, walked.end}, {trailing_start, trailing_end}
        };
        std::sort(pieces.begin(), pieces.end());

        cdx::SeqLen cursor = 0;
        for (const auto &[start, end]: pieces) {
            if (start == end) continue; // empty piece, nothing to check
            EXPECT_EQ(start, cursor) << "gap or overlap detected in node partition";
            cursor = end;
        }
        EXPECT_EQ(cursor, node_length);
    }
} // namespace

TEST(CoverageGapsPartitionTest, ForwardPartialMappingPartitionsWholeNode) {
    expectPartitionsWholeNode(100, 30, false, 40); // covers [30,70), leading [0,30), trailing [70,100)
}

TEST(CoverageGapsPartitionTest, ReversePartialMappingPartitionsWholeNode) {
    expectPartitionsWholeNode(100, 10, true, 50);
}

TEST(CoverageGapsPartitionTest, ForwardFullMappingPartitionsWholeNodeWithNoGaps) {
    expectPartitionsWholeNode(100, 0, false, 100);
}

TEST(CoverageGapsPartitionTest, ReverseFullMappingPartitionsWholeNodeWithNoGaps) {
    expectPartitionsWholeNode(100, 0, true, 100);
}
