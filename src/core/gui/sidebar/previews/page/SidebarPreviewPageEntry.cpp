#include "SidebarPreviewPageEntry.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <gdk/gdk.h>
#include <glib-object.h>

#include "control/Control.h"                                // for Control
#include "control/ScrollHandler.h"                          // for ScrollHan...
#include "control/settings/Settings.h"                      // for Settings
#include "gui/PagePreviewDecoration.h"                      // for Drawing  ...
#include "gui/sidebar/previews/page/SidebarPreviewPages.h"  // for SidebarPr...
#include "model/Document.h"
#include "util/Color.h"
#include "util/Util.h"
#include "util/gtk4_helper.h"

static const char* PAGE_ENTRY_DATA_KEY = "xopp-page-entry";

namespace {
const char* PAGE_DRAG_TARGET_NAME = "xournalpp/pages";

GtkTargetEntry pageDragTargets[] = {{const_cast<char*>(PAGE_DRAG_TARGET_NAME), GTK_TARGET_SAME_APP, 1}};
}  // namespace

SidebarPreviewPageEntry::SidebarPreviewPageEntry(SidebarPreviewPages* sidebar, const PageRef& page, size_t index):
        SidebarPreviewBaseEntry(sidebar, page), sidebar(sidebar), index(index) {
    if (sidebar->getControl()->getSettings()->getSidebarNumberingStyle() ==
        SidebarNumberingStyle::NUMBER_BELOW_PREVIEW) {
        gtk_widget_set_size_request(this->button.get(), imageWidth, imageHeight + PagePreviewDecoration::MARGIN_BOTTOM);
    }
    setupDragAndDrop();
    g_signal_connect(this->button.get(), "key-press-event",
                     G_CALLBACK(+[](GtkWidget*, GdkEventKey* event, gpointer self) -> gboolean {
                         return static_cast<SidebarPreviewPageEntry*>(self)->sidebar->onKeyPress(event);
                     }),
                     this);
}

SidebarPreviewPageEntry::~SidebarPreviewPageEntry() {
    GtkWidget* w = this->getWidget();
    if (w == nullptr) {
        return;
    }
    GtkWidget* parent = gtk_widget_get_parent(w);
    if (GTK_IS_FIXED(parent)) {
        gtk_fixed_remove(GTK_FIXED(parent), w);
    }
}

auto SidebarPreviewPageEntry::getRenderType() const -> PreviewRenderType { return RENDER_TYPE_PAGE_PREVIEW; }

bool SidebarPreviewPageEntry::handleButtonPress(GdkEventButton* event) {
    if (event->button == 3) {
        this->sidebar->handleContextClick(this->page);
        return true;
    }
    return false;
}

void SidebarPreviewPageEntry::mouseButtonPressCallback() {
    // A Ctrl/Shift press is handled before the drag source. "clicked" must not collapse that selection.
    if (this->sidebar->shouldIgnoreActivation()) {
        return;
    }
    GdkModifierType state = static_cast<GdkModifierType>(0);
    if (GdkEvent* current = gtk_get_current_event()) {
        gdk_event_get_state(current, &state);
        gdk_event_free(current);
    }
    if ((state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) != 0) {
        this->sidebar->handlePrimaryClick(this->page, state);
        return;
    }
    this->sidebar->activatePage(this->page);
    auto* control = sidebar->getControl();
    control->focusWindowFrom(this->getWidget());
    Document* doc = control->getDocument();
    doc->lock_shared();
    size_t pageIndex = doc->indexOf(this->page);
    doc->unlock_shared();
    if (pageIndex != npos && control->getCurrentPageNo() != pageIndex) {
        control->getScrollHandler()->jumpToPage(this->page);
    }
    control->firePageSelected(this->page);
}

void SidebarPreviewPageEntry::paint(cairo_t* cr) {
    SidebarPreviewBaseEntry::paint(cr);
    if (sidebar->getControl()->getSettings()->getSidebarNumberingStyle() != SidebarNumberingStyle::NONE) {
        drawEntryNumber(cr);
    }

    if (this->dropEdge == DropEdge::None) {
        return;
    }
    Util::cairo_set_source_rgbi(cr, sidebar->getControl()->getSettings()->getBorderColor());
    cairo_set_line_width(cr, 4);
    if (this->dropAlongX) {
        double x = this->dropEdge == DropEdge::Before ? 2.5 : getWidth() - 2.5;
        cairo_move_to(cr, x, 1);
        cairo_line_to(cr, x, getHeight() - 1);
    } else {
        double y = this->dropEdge == DropEdge::Before ? 2.5 : getHeight() - 2.5;
        cairo_move_to(cr, 1, y);
        cairo_line_to(cr, getWidth() - 1, y);
    }
    cairo_stroke(cr);
}

void SidebarPreviewPageEntry::drawEntryNumber(cairo_t* cr) {
    PagePreviewDecoration::drawDecoration(cr, this, this->sidebar->getControl());
}

auto SidebarPreviewPageEntry::getHeight() const -> int {
    if (sidebar->getControl()->getSettings()->getSidebarNumberingStyle() ==
        SidebarNumberingStyle::NUMBER_BELOW_PREVIEW) {
        return imageHeight + PagePreviewDecoration::MARGIN_BOTTOM;
    }
    return imageHeight;
}

void SidebarPreviewPageEntry::setIndex(size_t index) { this->index = index; }

size_t SidebarPreviewPageEntry::getIndex() const { return this->index; }

SidebarPreviewPageEntry* SidebarPreviewPageEntry::fromWidget(GtkWidget* widget) {
    if (widget == nullptr) {
        return nullptr;
    }
    return static_cast<SidebarPreviewPageEntry*>(g_object_get_data(G_OBJECT(widget), PAGE_ENTRY_DATA_KEY));
}

bool SidebarPreviewPageEntry::placeAfterAt(double x, double y) const {
    bool alongX = false;
    return dropEdgeAt(x, y, alongX) == DropEdge::After;
}

void SidebarPreviewPageEntry::showDropMarkerAt(double x, double y) {
    bool alongX = false;
    setDropEdge(dropEdgeAt(x, y, alongX), alongX);
}

bool SidebarPreviewPageEntry::isSelected() const { return this->selected; }

double SidebarPreviewPageEntry::getZoom() const { return this->sidebar->getZoom(); }

void SidebarPreviewPageEntry::setDropEdge(DropEdge edge, bool alongX) {
    if (this->dropEdge == edge && this->dropAlongX == alongX) {
        return;
    }
    this->dropEdge = edge;
    this->dropAlongX = alongX;
    gtk_widget_queue_draw(this->getWidget());
}

auto SidebarPreviewPageEntry::dropEdgeAt(double x, double y, bool& alongX) const -> DropEdge {
    const double width = std::max(1, getWidth());
    const double height = std::max(1, getHeight());
    const double relX = x / width;
    const double relY = y / height;
    alongX = std::abs(relX - 0.5) > std::abs(relY - 0.5);
    const bool before = alongX ? relX < 0.5 : relY < 0.5;
    return before ? DropEdge::Before : DropEdge::After;
}

void SidebarPreviewPageEntry::setupDragAndDrop() {
    GtkWidget* widget = this->button.get();
    g_object_set_data(G_OBJECT(widget), PAGE_ENTRY_DATA_KEY, this);

    // The drag source claims button 1 and stops the signal, so this must run first.
    g_signal_connect(widget, "button-press-event",
                     G_CALLBACK(+[](GtkWidget*, GdkEventButton* event, gpointer self) -> gboolean {
                         if (event->type != GDK_BUTTON_PRESS || event->button != 1) {
                             return false;
                         }
                         auto* entry = static_cast<SidebarPreviewPageEntry*>(self);
                         GdkModifierType state = static_cast<GdkModifierType>(event->state);
                         gdk_event_get_state(reinterpret_cast<GdkEvent*>(event), &state);
                         entry->sidebar->handlePrimaryClick(entry->getPage(), state);
                         return false;
                     }),
                     this);

    gtk_drag_source_set(widget, GDK_BUTTON1_MASK, pageDragTargets, 1, GDK_ACTION_MOVE);
    gtk_drag_dest_set(widget, static_cast<GtkDestDefaults>(0), pageDragTargets, 1, GDK_ACTION_MOVE);

    g_signal_connect(widget, "drag-begin", G_CALLBACK(+[](GtkWidget*, GdkDragContext* context, gpointer self) {
                         auto* entry = static_cast<SidebarPreviewPageEntry*>(self);
                         entry->sidebar->setDragInProgress(true);
                         gtk_drag_set_icon_name(context, "x-office-document", 0, 0);
                     }),
                     this);

    g_signal_connect(widget, "drag-data-get",
                     G_CALLBACK(+[](GtkWidget*, GdkDragContext*, GtkSelectionData* data, guint, guint, gpointer) {
                         const guchar marker = 1;
                         gtk_selection_data_set(data, gtk_selection_data_get_target(data), 8, &marker, 1);
                     }),
                     this);

    g_signal_connect(widget, "drag-drop",
                     G_CALLBACK(+[](GtkWidget* source, GdkDragContext* context, gint, gint, guint time,
                                    gpointer) -> gboolean {
                         GdkAtom target = gtk_drag_dest_find_target(source, context, nullptr);
                         if (target == GDK_NONE) {
                             return false;
                         }
                         gtk_drag_get_data(source, context, target, time);
                         return true;
                     }),
                     this);

    g_signal_connect(widget, "drag-motion",
                     G_CALLBACK(+[](GtkWidget*, GdkDragContext* context, gint x, gint y, guint time,
                                    gpointer self) -> gboolean {
                         auto* entry = static_cast<SidebarPreviewPageEntry*>(self);
                         bool alongX = false;
                         auto edge = entry->dropEdgeAt(x, y, alongX);
                         entry->setDropEdge(edge, alongX);
                         gdk_drag_status(context, GDK_ACTION_MOVE, time);
                         return true;
                     }),
                     this);

    g_signal_connect(widget, "drag-leave", G_CALLBACK(+[](GtkWidget*, GdkDragContext*, guint, gpointer self) {
                         static_cast<SidebarPreviewPageEntry*>(self)->setDropEdge(DropEdge::None, false);
                     }),
                     this);

    g_signal_connect(widget, "drag-end", G_CALLBACK(+[](GtkWidget*, GdkDragContext*, gpointer self) {
                         static_cast<SidebarPreviewPageEntry*>(self)->setDropEdge(DropEdge::None, false);
                     }),
                     this);

    g_signal_connect(widget, "drag-data-received",
                     G_CALLBACK(+[](GtkWidget*, GdkDragContext* context, gint x, gint y, GtkSelectionData*, guint,
                                    guint time, gpointer self) {
                         auto* entry = static_cast<SidebarPreviewPageEntry*>(self);
                         bool alongX = false;
                         auto edge = entry->dropEdgeAt(x, y, alongX);
                         entry->setDropEdge(DropEdge::None, false);

                         auto* source = SidebarPreviewPageEntry::fromWidget(gtk_drag_get_source_widget(context));
                         // Queue the move. Destroying the source or this thumbnail inside the drop handler
                         // frees it while GTK is still finishing the drag.
                         const bool placeAfter = edge == DropEdge::After;
                         bool handled = entry->sidebar->acceptPageDrop(source, entry->getIndex(), placeAfter);
                         gtk_drag_finish(context, handled, false, time);
                     }),
                     this);
}
