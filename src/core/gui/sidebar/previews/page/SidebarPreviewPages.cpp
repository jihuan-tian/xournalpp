#include "SidebarPreviewPages.h"

#include <algorithm>  // for max, find, remove_if, sort
#include <limits>
#include <memory>   // for uniqu...
#include <utility>  // for pair, move

#include <gdk/gdkkeysyms.h>
#include <glib-object.h>  // for g_obj...

#include "control/Control.h"                                    // for Control
#include "gui/MainWindow.h"                                     // for MainWindow
#include "gui/XournalView.h"                                    // for XournalView
#include "gui/sidebar/Sidebar.h"                                // for Sidebar
#include "gui/sidebar/previews/base/SidebarPreviewBaseEntry.h"  // for Sideb...
#include "model/Document.h"                                     // for Document
#include "model/PageRef.h"                                      // for PageRef
#include "model/XojPage.h"                                      // for XojPage
#include "util/Assert.h"                                        // for xoj_assert
#include "util/Util.h"                                          // for npos
#include "util/glib_casts.h"  // for wrap_for_once_v
#include "util/gtk4_helper.h"
#include "util/i18n.h"        // for _
#include "util/safe_casts.h"  // for as_signed

#include "SidebarPreviewPageEntry.h"  // for Sideb...

constexpr auto MENU_ID = "PreviewPagesContextMenu";
constexpr auto TOOLBAR_ID = "PreviewPagesToolbar";

SidebarPreviewPages::SidebarPreviewPages(Control* control, Sidebar* host):
        SidebarPreviewBase(control, MENU_ID, TOOLBAR_ID), iconNameHelper(control->getSettings()), host(host) {
    registerWindowActions();

    g_signal_connect(this->miniaturesContainer.get(), "key-press-event",
                     G_CALLBACK(+[](GtkWidget*, GdkEventKey* event, gpointer data) -> gboolean {
                         return static_cast<SidebarPreviewPages*>(data)->onKeyPress(event);
                     }),
                     this);
    gtk_widget_add_events(GTK_WIDGET(this->miniaturesContainer.get()), GDK_KEY_PRESS_MASK);
    setupContainerDrop();
}

SidebarPreviewPages::~SidebarPreviewPages() {
    if (this->host == nullptr || this->host->getMainWindow() == nullptr) {
        return;
    }
    GtkWidget* window = this->host->getMainWindow()->getWindow();
    if (window == nullptr) {
        return;
    }
    if (this->copyAction != nullptr) {
        g_action_map_remove_action(G_ACTION_MAP(window), "copy-pages");
        g_object_unref(this->copyAction);
        this->copyAction = nullptr;
    }
    if (this->pasteAction != nullptr) {
        g_action_map_remove_action(G_ACTION_MAP(window), "paste-pages");
        g_object_unref(this->pasteAction);
        this->pasteAction = nullptr;
    }
}

void SidebarPreviewPages::registerWindowActions() {
    if (this->host == nullptr || this->host->getMainWindow() == nullptr) {
        return;
    }
    GtkWidget* window = this->host->getMainWindow()->getWindow();
    if (window == nullptr) {
        return;
    }

    this->copyAction = g_simple_action_new("copy-pages", nullptr);
    g_signal_connect(this->copyAction, "activate", G_CALLBACK(+[](GSimpleAction*, GVariant*, gpointer data) {
                         static_cast<SidebarPreviewPages*>(data)->copySelection();
                     }),
                     this);
    g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(this->copyAction));

    this->pasteAction = g_simple_action_new("paste-pages", nullptr);
    g_signal_connect(this->pasteAction, "activate", G_CALLBACK(+[](GSimpleAction*, GVariant*, gpointer data) {
                         static_cast<SidebarPreviewPages*>(data)->pasteClipboard();
                     }),
                     this);
    g_simple_action_set_enabled(this->pasteAction, this->control->hasCopiedPages());
    g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(this->pasteAction));
}

namespace {
struct PendingPageMove {
    Control* control = nullptr;
    std::vector<PageRef> pages;
    size_t targetIndex = 0;
    bool placeAfter = false;
};

void runPendingPageMove(PendingPageMove* pending) {
    std::unique_ptr<PendingPageMove> owned(pending);
    if (owned->control != nullptr && !owned->pages.empty()) {
        owned->control->movePages(owned->pages, owned->targetIndex, owned->placeAfter);
    }
}

const char* PAGE_DRAG_TARGET_NAME = "xournalpp/pages";
GtkTargetEntry pageDragTargets[] = {{const_cast<char*>(PAGE_DRAG_TARGET_NAME), GTK_TARGET_SAME_APP, 1}};

void connectPageDropTarget(GtkWidget* widget, SidebarPreviewPages* sidebar) {
    if (widget == nullptr) {
        return;
    }
    gtk_drag_dest_set(widget, static_cast<GtkDestDefaults>(0), pageDragTargets, 1, GDK_ACTION_MOVE);
    // Without this, a drag GTK does not recognize (for example a file drop) is swallowed here.
    gtk_drag_dest_set_track_motion(widget, true);

    g_signal_connect(widget, "drag-motion",
                     G_CALLBACK(+[](GtkWidget* origin, GdkDragContext* context, gint x, gint y, guint time,
                                    gpointer self) -> gboolean {
                         if (gtk_drag_dest_find_target(origin, context, nullptr) == GDK_NONE) {
                             return false;
                         }
                         auto* pages = static_cast<SidebarPreviewPages*>(self);
                         pages->showDropMarker(origin, x, y);
                         gdk_drag_status(context, GDK_ACTION_MOVE, time);
                         return true;
                     }),
                     sidebar);

    g_signal_connect(widget, "drag-leave", G_CALLBACK(+[](GtkWidget*, GdkDragContext*, guint, gpointer self) {
                         static_cast<SidebarPreviewPages*>(self)->clearDropMarkers();
                     }),
                     sidebar);

    g_signal_connect(widget, "drag-drop",
                     G_CALLBACK(+[](GtkWidget* origin, GdkDragContext* context, gint, gint, guint time,
                                    gpointer) -> gboolean {
                         GdkAtom target = gtk_drag_dest_find_target(origin, context, nullptr);
                         if (target == GDK_NONE) {
                             return false;
                         }
                         gtk_drag_get_data(origin, context, target, time);
                         return true;
                     }),
                     sidebar);

    g_signal_connect(widget, "drag-data-received",
                     G_CALLBACK(+[](GtkWidget* origin, GdkDragContext* context, gint x, gint y, GtkSelectionData*, guint,
                                    guint time, gpointer self) {
                         auto* pages = static_cast<SidebarPreviewPages*>(self);
                         pages->clearDropMarkers();
                         auto* source = SidebarPreviewPageEntry::fromWidget(gtk_drag_get_source_widget(context));
                         const bool handled = pages->dropOnContainer(origin, x, y, source);
                         gtk_drag_finish(context, handled, false, time);
                     }),
                     sidebar);
}
}  // namespace

void SidebarPreviewPages::setupContainerDrop() {
    // The thumbnails do not cover the gap above the first page or below the last one.
    // Those releases would otherwise hit the main window and abort the drag.
    connectPageDropTarget(this->scrollableBox.get(), this);
    GtkWidget* fixed = GTK_WIDGET(this->miniaturesContainer.get());
    connectPageDropTarget(fixed, this);
    GtkWidget* parent = gtk_widget_get_parent(fixed);
    if (parent != nullptr && parent != this->scrollableBox.get()) {
        connectPageDropTarget(parent, this);
    }
}

void SidebarPreviewPages::queuePageMove(std::vector<PageRef> pages, size_t targetIndex, bool placeAfter) {
    if (pages.empty() || this->control == nullptr) {
        return;
    }
    auto* pending = new PendingPageMove{this->control, std::move(pages), targetIndex, placeAfter};
    // Later than the default idle so GTK can emit drag-end before thumbnails are destroyed.
    g_idle_add_full(G_PRIORITY_LOW, xoj::util::wrap_for_once_v<runPendingPageMove>, pending, nullptr);
}

bool SidebarPreviewPages::acceptPageDrop(SidebarPreviewPageEntry* source, size_t targetIndex, bool placeAfter) {
    if (source == nullptr || source->getSidebar() == nullptr ||
        source->getSidebar()->getControl() != this->control) {
        return false;
    }
    std::vector<PageRef> pages = source->getSidebar()->getSelectedPagesInOrder();
    if (pages.empty() || std::find(pages.begin(), pages.end(), source->getPage()) == pages.end()) {
        pages = {source->getPage()};
    }
    queuePageMove(std::move(pages), targetIndex, placeAfter);
    return true;
}

void SidebarPreviewPages::clearDropMarkers() {
    for (auto& preview: this->previews) {
        static_cast<SidebarPreviewPageEntry*>(preview.get())->setDropEdge(SidebarPreviewPageEntry::DropEdge::None,
                                                                           false);
    }
}

bool SidebarPreviewPages::findDropTarget(GtkWidget* origin, int x, int y, size_t& targetIndex, bool& placeAfter,
                                         SidebarPreviewPageEntry*& entry, double& localX, double& localY) {
    entry = nullptr;
    if (origin == nullptr || this->previews.empty()) {
        return false;
    }

    SidebarPreviewPageEntry* best = nullptr;
    double bestDistance = std::numeric_limits<double>::infinity();
    double bestX = 0;
    double bestY = 0;
    for (auto& preview: this->previews) {
        auto* candidate = static_cast<SidebarPreviewPageEntry*>(preview.get());
        int px = 0;
        int py = 0;
        if (!gtk_widget_translate_coordinates(origin, candidate->getWidget(), x, y, &px, &py)) {
            continue;
        }
        const double width = std::max(candidate->getWidth(), 1);
        const double height = std::max(candidate->getHeight(), 1);
        const double dx = px < 0 ? -px : (px > width ? px - width : 0);
        const double dy = py < 0 ? -py : (py > height ? py - height : 0);
        const double distance = dx * dx + dy * dy;
        if (distance < bestDistance) {
            bestDistance = distance;
            best = candidate;
            bestX = px;
            bestY = py;
        }
    }
    if (best == nullptr) {
        return false;
    }
    entry = best;
    localX = bestX;
    localY = bestY;
    targetIndex = best->getIndex();
    placeAfter = best->placeAfterAt(bestX, bestY);
    return true;
}

bool SidebarPreviewPages::dropOnContainer(GtkWidget* origin, int x, int y, SidebarPreviewPageEntry* source) {
    size_t targetIndex = 0;
    bool placeAfter = false;
    SidebarPreviewPageEntry* entry = nullptr;
    double localX = 0;
    double localY = 0;
    if (!findDropTarget(origin, x, y, targetIndex, placeAfter, entry, localX, localY)) {
        return false;
    }
    return acceptPageDrop(source, targetIndex, placeAfter);
}

void SidebarPreviewPages::showDropMarker(GtkWidget* origin, int x, int y) {
    size_t targetIndex = 0;
    bool placeAfter = false;
    SidebarPreviewPageEntry* entry = nullptr;
    double localX = 0;
    double localY = 0;
    if (!findDropTarget(origin, x, y, targetIndex, placeAfter, entry, localX, localY)) {
        clearDropMarkers();
        return;
    }
    for (auto& preview: this->previews) {
        auto* candidate = static_cast<SidebarPreviewPageEntry*>(preview.get());
        if (candidate == entry) {
            candidate->showDropMarkerAt(localX, localY);
        } else {
            candidate->setDropEdge(SidebarPreviewPageEntry::DropEdge::None, false);
        }
    }
}

void SidebarPreviewPages::enableSidebar() {
    SidebarPreviewBase::enableSidebar();

    pageSelected(this->selectedEntry);
}

auto SidebarPreviewPages::getName() -> std::string { return _("Page Preview"); }

auto SidebarPreviewPages::getIconName() -> std::string { return this->iconNameHelper.iconName("sidebar-page-preview"); }

void SidebarPreviewPages::updatePreviews() {
    this->selectedPages.clear();
    this->primaryPage.reset();
    this->anchorPage.reset();
    this->followsView = true;

    this->previews.clear();

    Document* doc = this->getControl()->getDocument();
    doc->lock_shared();
    size_t len = doc->getPageCount();
    for (size_t i = 0; i < len; i++) {
        auto p = std::make_unique<SidebarPreviewPageEntry>(this, doc->getPage(i), i);
        gtk_fixed_put(this->miniaturesContainer.get(), p->getWidget(), 0, 0);
        this->previews.emplace_back(std::move(p));
    }
    PageRef current;
    if (this->host != nullptr && this->host->getMainWindow() != nullptr &&
        this->host->getMainWindow()->getXournal() != nullptr) {
        size_t page = this->host->getMainWindow()->getXournal()->getCurrentPage();
        if (page < len) {
            current = doc->getPage(page);
        }
    }
    doc->unlock_shared();

    if (current) {
        selectOnly(current);
    }

    layout();
    updateActionState();
}

void SidebarPreviewPages::pageSizeChanged(size_t page) {
    if (page == npos || page >= this->previews.size()) {
        return;
    }
    auto& p = this->previews[page];
    p->updateSize();
    p->repaint();

    layout();
}

void SidebarPreviewPages::pageChanged(size_t page) {
    if (page == npos || page >= this->previews.size()) {
        return;
    }

    auto& p = this->previews[page];
    p->repaint();
}

void SidebarPreviewPages::pageDeleted(size_t page) {
    if (page >= this->previews.size()) {
        return;
    }

    previews.erase(previews.begin() + as_signed(page));
    updateIndices();
    // Keep PageRef selection. A move deletes a page and inserts it again; pruning here would drop it.
    applySelectionVisuals();
    layout();
}

void SidebarPreviewPages::pageInserted(size_t page) {
    Document* doc = control->getDocument();
    doc->lock_shared();
    PageRef inserted = doc->getPage(page);
    doc->unlock_shared();
    if (!inserted) {
        return;
    }
    // Appending is page == previews.size(). A larger index is not a position in this list.
    if (page > this->previews.size()) {
        page = this->previews.size();
    }
    auto p = std::make_unique<SidebarPreviewPageEntry>(this, inserted, page);

    gtk_fixed_put(this->miniaturesContainer.get(), p->getWidget(), 0, 0);
    this->previews.insert(this->previews.begin() + as_signed(page), std::move(p));

    updateIndices();
    applySelectionVisuals();
    layout();
}

void SidebarPreviewPages::selectPageNr(size_t page, size_t) { this->pageSelected(page); }

void SidebarPreviewPages::pageSelected(size_t page) {
    // Each window's preview list follows that window's selected page.
    if (this->host != nullptr && control->getSidebar() != this->host) {
        return;
    }
    // A multi-page selection stays put while the canvas scrolls.
    if (!this->followsView) {
        return;
    }

    Document* doc = this->control->getDocument();
    doc->lock_shared();
    PageRef selected;
    if (page != npos && page < doc->getPageCount()) {
        selected = doc->getPage(page);
    }
    doc->unlock_shared();

    if (!selected) {
        this->selectedPages.clear();
        this->primaryPage.reset();
        this->anchorPage.reset();
        this->selectedEntry = npos;
        applySelectionVisuals();
        return;
    }

    selectOnly(selected);
    this->selectedEntry = page;

    if (!this->enabled) {
        return;
    }

    scrollToPreview(this);
}

void SidebarPreviewPages::updateIndices() {
    size_t index = 0;
    for (auto& preview: this->previews) {
        static_cast<SidebarPreviewPageEntry*>(preview.get())->setIndex(index++);
    }
}

void SidebarPreviewPages::selectOnly(const PageRef& page) {
    this->selectedPages.clear();
    if (page) {
        this->selectedPages.push_back(page);
    }
    this->primaryPage = page;
    this->anchorPage = page;
    applySelectionVisuals();
    updateActionState();
}

void SidebarPreviewPages::toggleSelection(const PageRef& page) {
    if (!page) {
        return;
    }
    auto it = std::find(this->selectedPages.begin(), this->selectedPages.end(), page);
    if (it != this->selectedPages.end()) {
        this->selectedPages.erase(it);
        if (this->primaryPage == page) {
            this->primaryPage = this->selectedPages.empty() ? PageRef() : this->selectedPages.back();
        }
        if (this->anchorPage == page) {
            this->anchorPage = this->primaryPage;
        }
    } else {
        this->selectedPages.push_back(page);
        this->primaryPage = page;
        this->anchorPage = page;
    }
    applySelectionVisuals();
    updateActionState();
}

void SidebarPreviewPages::selectRangeTo(const PageRef& page) {
    PageRef anchor = this->anchorPage ? this->anchorPage : this->primaryPage;
    if (!anchor || !page) {
        selectOnly(page);
        return;
    }

    Document* doc = this->control->getDocument();
    doc->lock_shared();
    size_t from = doc->indexOf(anchor);
    size_t to = doc->indexOf(page);
    if (from == npos || to == npos) {
        doc->unlock_shared();
        selectOnly(page);
        return;
    }
    size_t lo = std::min(from, to);
    size_t hi = std::max(from, to);
    std::vector<PageRef> range;
    range.reserve(hi - lo + 1);
    for (size_t i = lo; i <= hi; i++) {
        range.push_back(doc->getPage(i));
    }
    doc->unlock_shared();

    this->selectedPages = std::move(range);
    this->primaryPage = page;
    applySelectionVisuals();
    updateActionState();
}

void SidebarPreviewPages::selectAll() {
    Document* doc = this->control->getDocument();
    doc->lock_shared();
    std::vector<PageRef> all;
    size_t count = doc->getPageCount();
    all.reserve(count);
    for (size_t i = 0; i < count; i++) {
        all.push_back(doc->getPage(i));
    }
    doc->unlock_shared();

    this->selectedPages = std::move(all);
    if (!this->primaryPage || !isSelected(this->primaryPage)) {
        this->primaryPage = this->selectedPages.empty() ? PageRef() : this->selectedPages.front();
    }
    if (!this->anchorPage || !isSelected(this->anchorPage)) {
        this->anchorPage = this->primaryPage;
    }
    this->followsView = this->selectedPages.size() <= 1;
    applySelectionVisuals();
    updateActionState();
}

void SidebarPreviewPages::setSelectedPages(std::vector<PageRef> pages, const PageRef& primary) {
    this->selectedPages = std::move(pages);
    this->primaryPage = primary ? primary : (this->selectedPages.empty() ? PageRef() : this->selectedPages.back());
    this->anchorPage = this->primaryPage;
    this->followsView = this->selectedPages.size() <= 1;
    applySelectionVisuals();
    updateActionState();
    if (this->enabled) {
        scrollToPreview(this);
    }
}

void SidebarPreviewPages::applySelectionVisuals() {
    Document* doc = this->control->getDocument();
    doc->lock_shared();
    for (auto& preview: this->previews) {
        auto* entry = static_cast<SidebarPreviewPageEntry*>(preview.get());
        entry->setSelected(isSelected(entry->getPage()));
    }
    this->selectedEntry = this->primaryPage ? doc->indexOf(this->primaryPage) : npos;
    doc->unlock_shared();
}

bool SidebarPreviewPages::isSelected(const PageRef& page) const {
    return std::find(this->selectedPages.begin(), this->selectedPages.end(), page) != this->selectedPages.end();
}

auto SidebarPreviewPages::getSelectedPagesInOrder() const -> std::vector<PageRef> {
    Document* doc = this->control->getDocument();
    doc->lock_shared();
    std::vector<std::pair<size_t, PageRef>> found;
    found.reserve(this->selectedPages.size());
    for (const PageRef& page: this->selectedPages) {
        size_t index = doc->indexOf(page);
        if (index != npos) {
            found.emplace_back(index, page);
        }
    }
    doc->unlock_shared();

    std::sort(found.begin(), found.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    found.erase(std::unique(found.begin(), found.end(),
                            [](const auto& a, const auto& b) { return a.first == b.first; }),
                found.end());

    std::vector<PageRef> ordered;
    ordered.reserve(found.size());
    for (auto& entry: found) {
        ordered.push_back(std::move(entry.second));
    }
    return ordered;
}

void SidebarPreviewPages::pruneSelection() {
    Document* doc = this->control->getDocument();
    doc->lock_shared();
    this->selectedPages.erase(std::remove_if(this->selectedPages.begin(), this->selectedPages.end(),
                                             [&](const PageRef& page) { return doc->indexOf(page) == npos; }),
                              this->selectedPages.end());
    if (this->primaryPage && doc->indexOf(this->primaryPage) == npos) {
        this->primaryPage = this->selectedPages.empty() ? PageRef() : this->selectedPages.back();
    }
    if (this->anchorPage && doc->indexOf(this->anchorPage) == npos) {
        this->anchorPage = this->primaryPage;
    }
    if (this->selectedPages.empty() && this->host != nullptr && this->host->getMainWindow() != nullptr &&
        this->host->getMainWindow()->getXournal() != nullptr) {
        size_t current = this->host->getMainWindow()->getXournal()->getCurrentPage();
        if (current < doc->getPageCount()) {
            this->primaryPage = doc->getPage(current);
            this->anchorPage = this->primaryPage;
            this->selectedPages.push_back(this->primaryPage);
            this->followsView = true;
        }
    }
    doc->unlock_shared();
    applySelectionVisuals();
    updateActionState();
    if (this->enabled) {
        scrollToPreview(this);
    }
}

void SidebarPreviewPages::copySelection() {
    auto pages = getSelectedPagesInOrder();
    if (pages.empty()) {
        return;
    }
    std::vector<PageRef> copies;
    copies.reserve(pages.size());
    Document* doc = this->control->getDocument();
    doc->lock_shared();
    for (const PageRef& page: pages) {
        copies.push_back(std::make_shared<XojPage>(*page));
    }
    doc->unlock_shared();
    this->control->setCopiedPages(std::move(copies));
}

auto SidebarPreviewPages::pasteTarget() const -> PageRef {
    if (this->primaryPage) {
        Document* doc = this->control->getDocument();
        doc->lock_shared();
        bool alive = doc->indexOf(this->primaryPage) != npos;
        doc->unlock_shared();
        if (alive) {
            return this->primaryPage;
        }
    }
    if (this->host != nullptr && this->host->getMainWindow() != nullptr &&
        this->host->getMainWindow()->getXournal() != nullptr) {
        size_t current = this->host->getMainWindow()->getXournal()->getCurrentPage();
        Document* doc = this->control->getDocument();
        doc->lock_shared();
        PageRef page;
        if (current < doc->getPageCount()) {
            page = doc->getPage(current);
        }
        doc->unlock_shared();
        return page;
    }
    return PageRef();
}

void SidebarPreviewPages::pasteClipboard() {
    if (!this->control->hasCopiedPages()) {
        return;
    }
    if (this->host != nullptr && this->host->getMainWindow() != nullptr) {
        this->control->focusWindowFrom(this->host->getMainWindow()->getWindow());
    }

    PageRef target = pasteTarget();
    Document* doc = this->control->getDocument();
    doc->lock_shared();
    size_t count = doc->getPageCount();
    size_t index = target ? doc->indexOf(target) : npos;
    doc->unlock_shared();
    size_t position = index == npos ? count : index + 1;

    std::vector<PageRef> fresh;
    const auto& clipboard = this->control->getCopiedPages();
    fresh.reserve(clipboard.size());
    for (const PageRef& page: clipboard) {
        fresh.push_back(std::make_shared<XojPage>(*page));
    }

    this->control->insertPages(fresh, position, true);
    if (!fresh.empty()) {
        setSelectedPages(fresh, fresh.front());
    }
}

void SidebarPreviewPages::activatePage(const PageRef& page) {
    this->followsView = true;
    selectOnly(page);
}

void SidebarPreviewPages::handlePrimaryClick(const PageRef& page, guint state) {
    // A previous drag may not have emitted "clicked". A new press starts clean.
    this->dragInProgress = false;

    const bool ctrl = (state & GDK_CONTROL_MASK) != 0;
    const bool shift = (state & GDK_SHIFT_MASK) != 0;
    if (ctrl || shift) {
        // Do this before focusing the window, or that focus change collapses the selection.
        this->modifierClick = true;
        this->followsView = false;
    }

    if (this->host != nullptr && this->host->getMainWindow() != nullptr) {
        this->control->focusWindowFrom(this->host->getMainWindow()->getWindow());
    }

    if (shift) {
        selectRangeTo(page);
    } else if (ctrl) {
        toggleSelection(page);
    } else {
        this->modifierClick = false;
        if (!isSelected(page)) {
            this->followsView = true;
            selectOnly(page);
        }
    }
}

void SidebarPreviewPages::handleContextClick(const PageRef& page) {
    if (this->host != nullptr && this->host->getMainWindow() != nullptr) {
        this->control->focusWindowFrom(this->host->getMainWindow()->getWindow());
    }
    if (!isSelected(page)) {
        this->followsView = true;
        selectOnly(page);
    } else {
        this->primaryPage = page;
        this->anchorPage = page;
        applySelectionVisuals();
    }
    updateActionState();
}

void SidebarPreviewPages::setDragInProgress(bool inProgress) {
    this->dragInProgress = inProgress;
    if (inProgress) {
        this->followsView = false;
    }
}

bool SidebarPreviewPages::shouldIgnoreActivation() {
    if (this->dragInProgress) {
        this->dragInProgress = false;
        return true;
    }
    if (this->modifierClick) {
        this->modifierClick = false;
        return true;
    }
    return false;
}

bool SidebarPreviewPages::onKeyPress(GdkEventKey* event) {
    const guint mods = event->state & gtk_accelerator_get_default_mod_mask();
    if (event->keyval == GDK_KEY_Delete || event->keyval == GDK_KEY_KP_Delete ||
        (event->keyval == GDK_KEY_BackSpace && mods == 0)) {
        if (this->host != nullptr && this->host->getMainWindow() != nullptr) {
            this->control->focusWindowFrom(this->host->getMainWindow()->getWindow());
        }
        this->control->deletePage();
        return true;
    }
    if ((event->keyval == GDK_KEY_c || event->keyval == GDK_KEY_C) && mods == GDK_CONTROL_MASK) {
        copySelection();
        return true;
    }
    if ((event->keyval == GDK_KEY_v || event->keyval == GDK_KEY_V) && mods == GDK_CONTROL_MASK) {
        pasteClipboard();
        return true;
    }
    if ((event->keyval == GDK_KEY_a || event->keyval == GDK_KEY_A) && mods == GDK_CONTROL_MASK) {
        selectAll();
        return true;
    }
    return false;
}

void SidebarPreviewPages::updateActionState() {
    if (this->copyAction != nullptr) {
        g_simple_action_set_enabled(this->copyAction, !getSelectedPagesInOrder().empty());
    }
    if (this->pasteAction != nullptr) {
        g_simple_action_set_enabled(this->pasteAction, this->control->hasCopiedPages());
    }
}
