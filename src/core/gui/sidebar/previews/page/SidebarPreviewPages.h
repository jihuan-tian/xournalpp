/*
 * Xournal++
 *
 * Previews of the pages in the document
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstddef>  // for size_t
#include <string>   // for string
#include <vector>   // for vector

#include <glib.h>     // for guint
#include <gtk/gtk.h>  // for GtkWidget, GdkEventKey

#include "gui/IconNameHelper.h"                            // for IconNameHe...
#include "gui/sidebar/previews/base/SidebarPreviewBase.h"  // for SidebarPre...
#include "model/PageRef.h"                                 // for PageRef

class Control;
class Sidebar;
class SidebarPreviewPageEntry;

class SidebarPreviewPages: public SidebarPreviewBase {
public:
    SidebarPreviewPages(Control* control, Sidebar* host);
    ~SidebarPreviewPages() override;

public:
    void enableSidebar() override;

    void selectPageNr(size_t page, size_t pdfPage) override;

    Sidebar* getHost() const { return this->host; }

    /**
     * @overwrite
     */
    std::string getName() override;

    /**
     * @overwrite
     */
    std::string getIconName() override;

    /**
     * Update the preview images
     * @overwrite
     */
    void updatePreviews() override;

    /// Pages in the current selection, in document order. Pages no longer in the document are skipped.
    std::vector<PageRef> getSelectedPagesInOrder() const;

    /// Copies the selection into the document page clipboard.
    void copySelection();
    /// Inserts clipboard pages after the primary selected page.
    void pasteClipboard();
    /// Enables Copy/Paste in this window from the current selection and clipboard.
    void refreshActionState() { updateActionState(); }
    /// Drops pages that are no longer in the document. An empty selection follows this window's current page.
    void pruneSelection();

    bool onKeyPress(GdkEventKey* event);

    void handlePrimaryClick(const PageRef& page, guint state);
    void handleContextClick(const PageRef& page);
    /// Moves pages after the current drag has finished, so thumbnails are not destroyed mid-drop.
    void queuePageMove(std::vector<PageRef> pages, size_t targetIndex, bool placeAfter);
    /// Drop from another preview in this application. Returns false when the drop is ignored.
    bool acceptPageDrop(SidebarPreviewPageEntry* source, size_t targetIndex, bool placeAfter);
    void clearDropMarkers();
    /// Highlight the preview edge nearest to a point on `origin`, including the gap past either end.
    void showDropMarker(GtkWidget* origin, int x, int y);
    /// Resolves a drop on the sidebar background (not on a thumbnail) and queues the move.
    bool dropOnContainer(GtkWidget* origin, int x, int y, SidebarPreviewPageEntry* source);
    /// Plain click release: keep only this page and show it in the view.
    void activatePage(const PageRef& page);
    /// Called when a drag of page previews starts or ends in this sidebar.
    void setDragInProgress(bool inProgress);
    bool shouldIgnoreActivation();

public:
    // DocumentListener interface (only the part which is not handled by SidebarPreviewBase)
    void pageSizeChanged(size_t page) override;
    void pageChanged(size_t page) override;
    void pageSelected(size_t page) override;
    void pageInserted(size_t page) override;
    void pageDeleted(size_t page) override;

private:
    void selectOnly(const PageRef& page);
    void toggleSelection(const PageRef& page);
    void selectRangeTo(const PageRef& page);
    void selectAll();
    void setSelectedPages(std::vector<PageRef> pages, const PageRef& primary);
    void applySelectionVisuals();
    void updateIndices();
    void updateActionState();
    void registerWindowActions();
    bool isSelected(const PageRef& page) const;
    PageRef pasteTarget() const;
    void setupContainerDrop();
    /// Nearest preview to a point on `origin`. Coordinates may fall outside every thumbnail.
    bool findDropTarget(GtkWidget* origin, int x, int y, size_t& targetIndex, bool& placeAfter,
                        SidebarPreviewPageEntry*& entry, double& localX, double& localY);

    /// When true, scrolling the view replaces the sidebar selection with that single page.
    bool followsView = true;
    bool modifierClick = false;
    bool dragInProgress = false;

    std::vector<PageRef> selectedPages;
    PageRef primaryPage;
    PageRef anchorPage;

    GSimpleAction* copyAction = nullptr;
    GSimpleAction* pasteAction = nullptr;

    IconNameHelper iconNameHelper;
    Sidebar* host = nullptr;
};
