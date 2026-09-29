/*
 * Xournal++
 *
 * A Sidebar preview widget
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include "gui/sidebar/previews/base/SidebarPreviewBaseEntry.h"  // for Previ...
#include "model/PageRef.h"                                      // for PageRef

class SidebarPreviewPages;

class SidebarPreviewPageEntry: public SidebarPreviewBaseEntry {
public:
    SidebarPreviewPageEntry(SidebarPreviewPages* sidebar, const PageRef& page, size_t index);
    ~SidebarPreviewPageEntry() override;

public:
    int getHeight() const override;

    PreviewRenderType getRenderType() const override;

    void setIndex(size_t index);
    size_t getIndex() const;

    bool isSelected() const;
    double getZoom() const;

    /// The preview that owns this button, or null.
    static SidebarPreviewPageEntry* fromWidget(GtkWidget* widget);

    /// True when a drop at these widget coordinates inserts after this page.
    /// Coordinates may lie outside the widget (the gap before the first page or after the last).
    bool placeAfterAt(double x, double y) const;
    void showDropMarkerAt(double x, double y);

    const PageRef& getPage() const { return this->page; }
    SidebarPreviewPages* getSidebar() const { return this->sidebar; }

    enum class DropEdge { None, Before, After };
    void setDropEdge(DropEdge edge, bool alongX);

protected:
    SidebarPreviewPages* sidebar;
    void mouseButtonPressCallback() override;
    bool handleButtonPress(GdkEventButton* event) override;
    void paint(cairo_t* cr) override;

private:
    size_t index;
    friend class PreviewJob;

    void drawEntryNumber(cairo_t* cr);
    void setupDragAndDrop();
    DropEdge dropEdgeAt(double x, double y, bool& alongX) const;

    DropEdge dropEdge = DropEdge::None;
    bool dropAlongX = false;
};
