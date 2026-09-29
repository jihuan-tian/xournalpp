/*
 * Xournal++
 *
 * Undo action for inserting or deleting several pages at once.
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

class InsertDeletePagesUndoAction: public UndoAction {
public:
    /// `positions` is parallel to `pages` and sorted ascending.
    InsertDeletePagesUndoAction(std::vector<PageRef> pages, std::vector<size_t> positions, bool inserted);
    ~InsertDeletePagesUndoAction() override = default;

public:
    bool undo(Control* control) override;
    bool redo(Control* control) override;

    std::vector<PageRef> getPages() override;
    std::string getText() override;

private:
    std::vector<PageRef> pages;
    std::vector<size_t> positions;
    bool inserted;
};
