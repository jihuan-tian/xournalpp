#include "ToolEraserSlider.h"

#include <cmath>
#include <utility>

#include <glib.h>

#include "control/ToolHandler.h"
#include "control/actions/ActionDatabase.h"
#include "util/GVariantTemplate.h"
#include "util/gtk4_helper.h"
#include "util/i18n.h"
#include "util/raii/GVariantSPtr.h"

#include "SliderItemCreationHelper.h"

namespace {
constexpr int FINE_STEP_COUNT = 200;
constexpr int COARSE_STEP_COUNT = 20;

constexpr auto SLIDER_RANGE = AbstractSliderItem::SliderRange{
        ToolHandler::ERASER_MIN_RADIUS, ToolHandler::ERASER_MAX_RADIUS, FINE_STEP_COUNT, COARSE_STEP_COUNT};

void setLabelFromAction(GObject* action, GtkLabel* label) {
    xoj::util::GVariantSPtr state(g_action_get_state(G_ACTION(action)), xoj::util::adopt);
    const double radius = getGVariantValue<double>(state.get());
    gtk_label_set_text(label, ToolEraserSlider::formatRadius(radius).c_str());
}
}  // namespace

ToolEraserSlider::ToolEraserSlider(std::string id, IconNameHelper iconNameHelper, ActionDatabase& db):
        AbstractSliderItem{std::move(id), Category::TOOLS, SLIDER_RANGE, db.getAction(Action::TOOL_ERASER_RADIUS)},
        iconName(iconNameHelper.iconName("tool-eraser")) {}

auto ToolEraserSlider::createItem(bool horizontal) -> xoj::util::WidgetSPtr {
    auto scale = SliderItemCreationHelper<ToolEraserSlider>::createItem(this, horizontal);
    if (horizontal) {
        gtk_widget_set_size_request(scale.get(), 160, -1);
        gtk_widget_set_valign(scale.get(), GTK_ALIGN_CENTER);
    } else {
        gtk_widget_set_size_request(scale.get(), -1, 160);
        gtk_widget_set_halign(scale.get(), GTK_ALIGN_CENTER);
    }

    GtkWidget* valueLabel = gtk_label_new("");
    gtk_label_set_width_chars(GTK_LABEL(valueLabel), 9);
    gtk_label_set_xalign(GTK_LABEL(valueLabel), 0.0);
    setLabelFromAction(G_OBJECT(this->gAction.get()), GTK_LABEL(valueLabel));
    g_signal_connect_object(this->gAction.get(), "notify::state",
                            G_CALLBACK(+[](GObject* action, GParamSpec*, gpointer label) {
                                setLabelFromAction(action, GTK_LABEL(label));
                            }),
                            valueLabel, GConnectFlags(0));

    const auto orientation = horizontal ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;
    xoj::util::WidgetSPtr box(gtk_box_new(orientation, 6), xoj::util::adopt);
    gtk_box_append(GTK_BOX(box.get()), scale.get());
    gtk_box_append(GTK_BOX(box.get()), valueLabel);
    gtk_widget_set_tooltip_text(box.get(), _("Eraser radius"));
    return box;
}

auto ToolEraserSlider::getToolDisplayName() const -> std::string { return _("Eraser Radius"); }

auto ToolEraserSlider::getNewToolIcon() const -> GtkWidget* {
    return gtk_image_new_from_icon_name(iconName.c_str(), GTK_ICON_SIZE_SMALL_TOOLBAR);
}

auto ToolEraserSlider::scaleFunction(double radius) -> double { return std::log(radius); }

auto ToolEraserSlider::scaleInverseFunction(double sliderValue) -> double { return std::exp(sliderValue); }

auto ToolEraserSlider::formatRadius(double radius) -> std::string {
    char buf[32];
    if (radius >= 10.0) {
        g_snprintf(buf, sizeof(buf), "%.1f pt", radius);
    } else {
        g_snprintf(buf, sizeof(buf), "%.2f pt", radius);
    }
    return buf;
}
