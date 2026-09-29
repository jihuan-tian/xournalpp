#include "InsertDeletePagesUndoAction.h"

#include "control/Control.h"
#include "util/i18n.h"

InsertDeletePagesUndoAction::InsertDeletePagesUndoAction(std::vector<PageRef> pages, std::vector<size_t> positions,
                                                         bool inserted):
        UndoAction("InsertDeletePagesUndoAction"),
        pages(std::move(pages)),
        positions(std::move(positions)),
        inserted(inserted) {}

auto InsertDeletePagesUndoAction::undo(Control* control) -> bool {
    if (this->inserted) {
        control->runPageStructureChange([&] { control->removePages(this->pages); });
    } else {
        control->runPageStructureChange(
                [&] { control->insertPagesAtPositions(this->pages, this->positions); });
    }
    control->prunePageSelections();
    return true;
}

auto InsertDeletePagesUndoAction::redo(Control* control) -> bool {
    if (this->inserted) {
        control->runPageStructureChange(
                [&] { control->insertPagesAtPositions(this->pages, this->positions); });
    } else {
        control->runPageStructureChange([&] { control->removePages(this->pages); });
    }
    control->prunePageSelections();
    return true;
}

auto InsertDeletePagesUndoAction::getPages() -> std::vector<PageRef> { return this->pages; }

auto InsertDeletePagesUndoAction::getText() -> std::string {
    if (this->inserted) {
        return this->pages.size() == 1 ? _("Page inserted") : _("Pages inserted");
    }
    return this->pages.size() == 1 ? _("Page deleted") : _("Pages deleted");
}
