#include <array>
#include <map>

#include <gtest/gtest.h>

#include "model/Stroke.h"
#include "model/eraser/ErasableStroke.h"
#include "model/eraser/ErasableStrokeOverlapTree.h"
#include "model/eraser/PaddedBox.h"
#include "util/Rectangle.h"
#include "util/SmallVector.h"

using xoj::util::Rectangle;

void assertRangesEq(const Range& r1, const Range& r2) {
    ASSERT_EQ(r1.minX, r2.minX);
    ASSERT_EQ(r1.minY, r2.minY);
    ASSERT_EQ(r1.maxX, r2.maxX);
    ASSERT_EQ(r1.maxY, r2.maxY);
}

TEST(ErasableStroke, testOverlapTree) {

    // clang format off
    std::vector<Point> testPath = {{0, 0},  {2, 2},  {5, 2}, {7, 4}, {3, 6}, {2, 8},
                                   {5, 11}, {7, 10}, {7, 6}, {6, 7}, {4, 4}, {1, 3}};
    // clang format on

    Stroke stroke;
    std::vector<Point>& strokePoints = const_cast<std::vector<Point>&>(stroke.getPointVector());
    strokePoints.swap(testPath);

    stroke.setWidth(2);
    stroke.setFill(-1);
    stroke.setToolType(StrokeTool::PEN);

    std::array<ErasableStroke::OverlapTree, 6> trees;
    trees[0].populate({{0, 0.0}, {3, 0.5}}, stroke);
    trees[1].populate({{2, 0.5}, {5, 1.0}}, stroke);
    trees[2].populate({{0, 0.5}, {0, 0.75}}, stroke);
    trees[3].populate({{5, 0.0}, {9, 0.0}}, stroke);
    trees[4].populate({{7, 0.5}, {9, 1.0}}, stroke);
    trees[5].populate({{8, 0.0}, {10, 1.0}}, stroke);

    auto makeRange = [](double x1, double y1, double width, double height) {
        Range r(x1, y1);
        r.addPoint(x1 + width, y1 + height);
        return r;
    };

    // clang format off
    Range overlapBoxes[2][5] = {{
                                        makeRange(4, 2, 4, 4),      // 0 and 1
                                        makeRange(0, 0, 2.5, 2.5),  // 0 and 2
                                        makeRange(5, 5, 3, 1),      // 0 and 3
                                        makeRange(4, 3, 4, 3),      // 0 and 4
                                        makeRange(0, 2, 8, 4)       // 0 and 5
                                },
                                {
                                        Range(0, 0),            // unused
                                        Range(-10000, -10000),  // 1 and 2 don't overlap
                                        makeRange(1, 5, 7, 7),  // 1 and 3
                                        makeRange(3, 3, 5, 5),  // 1 and 4
                                        makeRange(2, 3, 6, 5)   // 1 and 5
                                }};
    // clang format on

    auto singlePointRangeAtCenter = [](Range range) {
        return Range(0.5 * (range.minX + range.maxX), 0.5 * (range.minY + range.maxY));
    };

    for (size_t i = 0; i < 2; i++) {
        for (size_t j = i; j < 5; j++) {
            Range range = singlePointRangeAtCenter(overlapBoxes[i][j]);
            trees[i].addOverlapsToRange(trees[j + 1], 1, range);
            assertRangesEq(range, overlapBoxes[i][j]);
        }
    }
}

TEST(ErasableStroke, testGetStrokes) {
    std::array<Stroke, 3> strokes;

    // Normal stroke
    strokes[0].addPoint({0, 0, 2});
    strokes[0].addPoint({2, 2, 2.5});
    strokes[0].addPoint({5, 2, 3});
    strokes[0].addPoint({7, 4, 2.5});
    strokes[0].addPoint({3, 6, 2});
    strokes[0].addPoint({2, 8, 1.5});
    strokes[0].addPoint({5, 11, 1});
    strokes[0].addPoint({7, 10, 1.5});
    strokes[0].addPoint({7, 6, 2});
    strokes[0].addPoint({6, 7, 2.5});
    strokes[0].addPoint({4, 4, 3});
    strokes[0].addPoint({1, 3});
    strokes[0].setWidth(3);
    strokes[0].setFill(-1);
    strokes[0].setToolType(StrokeTool::PEN);

    // Closed stroke. Filled. Audio
    strokes[1].addPoint({0, 0, 2});
    strokes[1].addPoint({2, 2, 2.5});
    strokes[1].addPoint({5, 2, 3});
    strokes[1].addPoint({1, 4, 2.5});
    strokes[1].addPoint({3, 6, 2});
    strokes[1].addPoint({0, 0});
    strokes[1].setWidth(3);
    strokes[1].setFill(123);
    strokes[1].setToolType(StrokeTool::PEN);
    strokes[1].setAudioFilename("assets/bar.mp3");

    // Closed stroke. Highlighter.
    strokes[2].addPoint({0, 0, 2});
    strokes[2].addPoint({2, 2, 2.5});
    strokes[2].addPoint({5, 2, 3});
    strokes[2].addPoint({1, 4, 2.5});
    strokes[2].addPoint({3, 6, 2});
    strokes[2].addPoint({0, 0});
    strokes[2].setWidth(3);
    strokes[2].setFill(-1);
    strokes[2].setToolType(StrokeTool::HIGHLIGHTER);

    std::array<ErasableStroke, 3> erasables = {ErasableStroke(strokes[0]), ErasableStroke(strokes[1]),
                                               ErasableStroke(strokes[2])};

    std::array<bool, 3> areClosed = {false, true, true};

    std::array<IntersectionParametersContainer, 3> fakeIntersections;
    fakeIntersections[0] = {{1U, 0.5}, {2U, 0.5}, {3U, 0.5}, {4U, 0.5}};
    fakeIntersections[1] = {{1U, 0.5}, {2U, 0.5}, {3U, 0.5}, {4U, 0.5}};
    fakeIntersections[2] = {{0U, 0.0}, {2U, 0.5}, {3U, 0.5}, {4U, 1.0}};

    std::array<std::vector<std::vector<Point>>, 3> resultingPaths;
    resultingPaths[0] = std::vector<std::vector<Point>>(3);
    resultingPaths[0][0] = {{0, 0, 2}, {2, 2, 2.5}, {3.5, 2}};
    resultingPaths[0][1] = {{6, 3, 3}, {7, 4, 2.5}, {5, 5}};
    resultingPaths[0][2] = {{2.5, 7, 2}, {2, 8, 1.5}, {5, 11, 1}, {7, 10, 1.5},
                            {7, 6, 2},   {6, 7, 2.5}, {4, 4, 3},  {1, 3}};

    resultingPaths[1] = std::vector<std::vector<Point>>(2);
    resultingPaths[1][0] = {{1.5, 3, 2}, {0, 0, 2}, {2, 2, 2.5}, {3.5, 2}};
    resultingPaths[1][1] = {{3, 3, 3}, {1, 4, 2.5}, {2, 5}};

    resultingPaths[2] = std::vector<std::vector<Point>>(1);
    resultingPaths[2][0] = {{3, 3, 3}, {1, 4, 2.5}, {2, 5}};

    unsigned int i = 0;
    for (auto& erasable: erasables) {
        strokes[i].setErasable(&erasable);
        Range range(0, 0);
        erasable.beginErasure(fakeIntersections[i], range);
        ASSERT_EQ(erasable.isClosedStroke(), areClosed[i]);
        auto res = erasable.getStrokes();
        ASSERT_EQ(res.size(), resultingPaths[i].size());
        for (unsigned int j = 0; j < res.size(); ++j) {
            ASSERT_EQ(res[j]->getWidth(), strokes[i].getWidth());
            ASSERT_EQ(res[j]->getFill(), strokes[i].getFill());
            ASSERT_EQ(res[j]->getToolType(), strokes[i].getToolType());
            ASSERT_EQ(res[j]->getAudioFilename(), strokes[i].getAudioFilename());
            const auto& pts1 = res[j]->getPointVector();
            const auto& pts2 = resultingPaths[i][j];
            ASSERT_EQ(pts1.size(), pts2.size());
            for (unsigned int k = 0; k < pts1.size(); ++k) {
                ASSERT_EQ(pts1[k].x, pts2[k].x);
                ASSERT_EQ(pts1[k].y, pts2[k].y);
                ASSERT_EQ(pts1[k].z, pts2[k].z);
            }
        }
        ++i;
    }
}


TEST(ErasableStroke, testIntersectWithPaddedCircle) {
    // Straight stroke along y = 0 from x = 0 to x = 100.
    Stroke stroke;
    stroke.addPoint({0, 0});
    stroke.addPoint({100, 0});

    // Disk centered on the stroke. Crossings at x = 40 and x = 60.
    {
        PaddedBox disk{{50, 0}, 10, 10};
        auto res = stroke.intersectWithPaddedBox(disk);
        ASSERT_EQ(res.size(), 2U);
        EXPECT_EQ(res[0].index, 0U);
        EXPECT_DOUBLE_EQ(res[0].t, 0.4);
        EXPECT_EQ(res[1].index, 0U);
        EXPECT_DOUBLE_EQ(res[1].t, 0.6);
    }

    // Stroke begins at the center and leaves the disk at x = 60, t = 1/3 of (50 -> 80).
    Stroke fromCenter;
    fromCenter.addPoint({50, 0});
    fromCenter.addPoint({80, 0});
    {
        PaddedBox disk{{50, 0}, 10, 10};
        auto res = fromCenter.intersectWithPaddedBox(disk);
        ASSERT_EQ(res.size(), 2U);
        EXPECT_EQ(res[0].index, 0U);
        EXPECT_DOUBLE_EQ(res[0].t, 0.0);
        EXPECT_EQ(res[1].index, 0U);
        EXPECT_DOUBLE_EQ(res[1].t, 1.0 / 3.0);
    }

    // Outer disk is hit, inner disk is not: nothing is erased.
    {
        PaddedBox disk{{50, 20}, 5, 25};
        auto res = stroke.intersectWithPaddedBox(disk);
        EXPECT_TRUE(res.empty());
    }

    // Starts in the padding, passes through the inner disk, then leaves.
    // Outer radius 15 crosses at x = 35 and x = 65. The stroke starts at x = 40.
    Stroke fromPadding;
    fromPadding.addPoint({40, 0});
    fromPadding.addPoint({100, 0});
    {
        PaddedBox disk{{50, 0}, 5, 15};
        auto res = fromPadding.intersectWithPaddedBox(disk);
        ASSERT_EQ(res.size(), 2U);
        EXPECT_EQ(res[0].index, 0U);
        EXPECT_DOUBLE_EQ(res[0].t, 0.0);
        EXPECT_EQ(res[1].index, 0U);
        EXPECT_DOUBLE_EQ(res[1].t, 25.0 / 60.0);
    }
}

TEST(Stroke, eraserFootprintIsACircle) {
    // (60, 0) lies in the corner of a square of half-side 10 centered at (50, 10),
    // and outside the disk of radius 10.
    Stroke corner;
    corner.addPoint({60, 0});
    corner.addPoint({80, 0});
    EXPECT_FALSE(corner.intersects(50, 10, 10));

    Stroke hit;
    hit.addPoint({50, 0});
    hit.addPoint({80, 0});
    EXPECT_TRUE(hit.intersects(50, 10, 10));

    PaddedBox disk{{50, 10}, 10, 10};
    EXPECT_TRUE(corner.intersectWithPaddedBox(disk).empty());
}
