#include "ToolThicknessSlider.h"

#include <cmath>
#include <utility>

#include <glib.h>

#include "control/actions/ActionDatabase.h"
#include "util/GVariantTemplate.h"
#include "util/gtk4_helper.h"
#include "util/i18n.h"
#include "util/raii/GVariantSPtr.h"

#include "SliderItemCreationHelper.h"

namespace {
constexpr int FINE_STEP_COUNT = 200;
constexpr int COARSE_STEP_COUNT = 20;

void setLabelFromAction(GObject* action, GtkLabel* label) {
    xoj::util::GVariantSPtr state(g_action_get_state(G_ACTION(action)), xoj::util::adopt);
    const double thickness = getGVariantValue<double>(state.get());
    gtk_label_set_text(label, ToolThicknessSlider::formatThickness(thickness).c_str());
}
}  // namespace

ToolThicknessSlider::ToolThicknessSlider(std::string id, IconNameHelper iconNameHelper, ActionDatabase& db,
                                         Action action, double min, double max, const char* icon, std::string tooltip,
                                         std::string displayName):
        AbstractSliderItem{std::move(id), Category::TOOLS, SliderRange{min, max, FINE_STEP_COUNT, COARSE_STEP_COUNT},
                           db.getAction(action)},
        iconName(iconNameHelper.iconName(icon)),
        tooltip(std::move(tooltip)),
        displayName(std::move(displayName)) {}

auto ToolThicknessSlider::createItem(bool horizontal) -> xoj::util::WidgetSPtr {
    auto scale = SliderItemCreationHelper<ToolThicknessSlider>::createItem(this, horizontal);
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
    gtk_widget_set_tooltip_text(box.get(), this->tooltip.c_str());
    return box;
}

auto ToolThicknessSlider::getToolDisplayName() const -> std::string { return this->displayName; }

auto ToolThicknessSlider::getNewToolIcon() const -> GtkWidget* {
    return gtk_image_new_from_icon_name(iconName.c_str(), GTK_ICON_SIZE_SMALL_TOOLBAR);
}

auto ToolThicknessSlider::scaleFunction(double thickness) -> double { return std::log(thickness); }

auto ToolThicknessSlider::scaleInverseFunction(double sliderValue) -> double { return std::exp(sliderValue); }

auto ToolThicknessSlider::formatThickness(double thickness) -> std::string {
    char buf[32];
    if (thickness >= 10.0) {
        g_snprintf(buf, sizeof(buf), "%.1f pt", thickness);
    } else {
        g_snprintf(buf, sizeof(buf), "%.2f pt", thickness);
    }
    return buf;
}
