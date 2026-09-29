#include "MovePagesUndoAction.h"

#include "control/Control.h"
#include "util/i18n.h"

MovePagesUndoAction::MovePagesUndoAction(std::vector<PageRef> pages, std::vector<size_t> originalIndices,
                                         size_t insertAt):
        UndoAction("MovePagesUndoAction"),
        movedPages(std::move(pages)),
        originalIndices(std::move(originalIndices)),
        insertAt(insertAt) {}

auto MovePagesUndoAction::undo(Control* control) -> bool {
    control->runPageStructureChange([&] {
        control->removePages(this->movedPages);
        control->insertPagesAtPositions(this->movedPages, this->originalIndices);
    });
    return true;
}

auto MovePagesUndoAction::redo(Control* control) -> bool {
    control->runPageStructureChange([&] {
        control->removePages(this->movedPages);
        control->insertPagesAt(this->insertAt, this->movedPages);
    });
    return true;
}

auto MovePagesUndoAction::getPages() -> std::vector<PageRef> { return this->movedPages; }

auto MovePagesUndoAction::getText() -> std::string {
    if (this->movedPages.size() == 1) {
        return _("Move page");
    }
    return _("Move pages");
}
