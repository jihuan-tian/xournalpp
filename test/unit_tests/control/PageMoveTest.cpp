#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "control/PageMove.h"

TEST(PageMove, noopWhenBlockAlreadyAtDestination) {
    // Pages 0 and 1 are already immediately before page 2.
    EXPECT_EQ(pageMoveInsertionIndex({0, 1}, 2, false), std::nullopt);
    // Dropping after a page inside the same contiguous block does not reorder it.
    EXPECT_EQ(pageMoveInsertionIndex({1, 2, 3}, 2, true), std::nullopt);
}

TEST(PageMove, movesBlockAfterTarget) {
    // [0, 1, 2, 3, 4], move 1 and 2 to after 4 → [0, 3, 4, 1, 2], first inserted at 3.
    EXPECT_EQ(pageMoveInsertionIndex({1, 2}, 4, true), std::optional<size_t>(3));
}

TEST(PageMove, movesPageBeforeEarlierTarget) {
    // [0, 1, 2, 3, 4], move 3 to before 1 → [0, 3, 1, 2, 4], inserted at 1.
    EXPECT_EQ(pageMoveInsertionIndex({3}, 1, false), std::optional<size_t>(1));
}

TEST(PageMove, gathersNonContiguousPagesAfterTarget) {
    // [0, 1, 2, 3], move 0 and 2 to after 1 → [1, 0, 2, 3], inserted at 1.
    EXPECT_EQ(pageMoveInsertionIndex({0, 2}, 1, true), std::optional<size_t>(1));
}

TEST(PageMove, emptySelectionDoesNothing) {
    EXPECT_EQ(pageMoveInsertionIndex({}, 0, true), std::nullopt);
}

TEST(PageMove, appendsAfterLastPage) {
    // [0, 1, 2, 3], move 0 to after 3 → [1, 2, 3, 0], inserted at 3.
    EXPECT_EQ(pageMoveInsertionIndex({0}, 3, true), std::optional<size_t>(3));
    // [0, 1, 2, 3], move 1 and 2 to after 3 → [0, 3, 1, 2], inserted at 2.
    EXPECT_EQ(pageMoveInsertionIndex({1, 2}, 3, true), std::optional<size_t>(2));
    // The block is already at the end.
    EXPECT_EQ(pageMoveInsertionIndex({2, 3}, 3, true), std::nullopt);
}

TEST(PageMove, insertsBeforeFirstPage) {
    // [0, 1, 2, 3], move 2 to before 0 → [2, 0, 1, 3], inserted at 0.
    EXPECT_EQ(pageMoveInsertionIndex({2}, 0, false), std::optional<size_t>(0));
    // [0, 1, 2, 3], move 1 and 3 to before 0 → [1, 3, 0, 2], inserted at 0.
    EXPECT_EQ(pageMoveInsertionIndex({1, 3}, 0, false), std::optional<size_t>(0));
    // The block is already at the start.
    EXPECT_EQ(pageMoveInsertionIndex({0, 1}, 0, false), std::nullopt);
}
