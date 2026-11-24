/**
 * Xournal++
 *
 * @brief Circular eraser footprint, plus the padding used to catch thick strokes.
 *
 * halfSize is the inner radius: the disk that is actually erased.
 * halfSizeWithPadding is the outer radius: the inner radius plus a margin of the stroke width.
 * The rectangles below are the axis-aligned bounds of those disks. They are used to repaint
 * and to skip strokes that cannot meet the eraser.
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include "model/Point.h"
#include "util/Rectangle.h"

struct PaddedBox {
    Point center;
    /// Inner radius of the circular eraser.
    double halfSize;
    /// Outer radius, including stroke-width padding.
    double halfSizeWithPadding;

    xoj::util::Rectangle<double> getInnerRectangle() const {
        return {center.x - halfSize, center.y - halfSize, 2 * halfSize, 2 * halfSize};
    }
    xoj::util::Rectangle<double> getOuterRectangle() const {
        return {center.x - halfSizeWithPadding, center.y - halfSizeWithPadding, 2 * halfSizeWithPadding,
                2 * halfSizeWithPadding};
    }

    void addToRange(Range& range) const {
        range.addPoint(center.x - halfSizeWithPadding, center.y - halfSizeWithPadding);
        range.addPoint(center.x + halfSizeWithPadding, center.y + halfSizeWithPadding);
    }
};
