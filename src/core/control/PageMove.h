/*
 * Xournal++
 *
 * Pure helpers for moving a set of pages.
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <vector>

/**
 * Where a block of pages lands after they are removed, given a drop on `targetIndex`.
 *
 * `sortedIndices` is the current indices of the pages to move, ascending and unique.
 * `placeAfter` inserts after the target page; otherwise the pages are inserted before it.
 * Returns nullopt when the move would leave the document unchanged.
 */
inline std::optional<size_t> pageMoveInsertionIndex(const std::vector<size_t>& sortedIndices, size_t targetIndex,
                                                    bool placeAfter) {
    if (sortedIndices.empty()) {
        return std::nullopt;
    }

    const size_t dest = placeAfter ? targetIndex + 1 : targetIndex;
    const size_t removedBefore =
            static_cast<size_t>(std::count_if(sortedIndices.begin(), sortedIndices.end(),
                                              [dest](size_t index) { return index < dest; }));
    const size_t insertAt = dest - removedBefore;

    bool unchanged = sortedIndices.size() > 0;
    for (size_t n = 0; unchanged && n < sortedIndices.size(); n++) {
        unchanged = sortedIndices[n] == insertAt + n;
    }
    if (unchanged) {
        return std::nullopt;
    }
    return insertAt;
}
