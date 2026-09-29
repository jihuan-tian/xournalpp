/*
 * Xournal++
 *
 * Undo action for moving one or more pages.
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "model/PageRef.h"

#include "UndoAction.h"

class Control;

class MovePagesUndoAction: public UndoAction {
public:
    MovePagesUndoAction(std::vector<PageRef> pages, std::vector<size_t> originalIndices, size_t insertAt);
    ~MovePagesUndoAction() override = default;

public:
    bool undo(Control* control) override;
    bool redo(Control* control) override;

    std::vector<PageRef> getPages() override;
    std::string getText() override;

private:
    std::vector<PageRef> movedPages;
    /// Indices the pages occupied before the move, parallel to movedPages and sorted ascending.
    std::vector<size_t> originalIndices;
    /// Index of the first moved page after the pages were removed from their old places.
    size_t insertAt;
};
