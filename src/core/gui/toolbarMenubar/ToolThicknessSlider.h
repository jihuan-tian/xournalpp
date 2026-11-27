/*
 * Xournal++
 *
 * Toolbar slider for pen, eraser, or highlighter thickness.
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string>

#include "enums/Action.enum.h"
#include "gui/IconNameHelper.h"

#include "AbstractSliderItem.h"

class ActionDatabase;

class ToolThicknessSlider: public AbstractSliderItem {
public:
    ToolThicknessSlider(std::string id, IconNameHelper iconNameHelper, ActionDatabase& db, Action action, double min,
                        double max, const char* icon, std::string tooltip, std::string displayName);
    ~ToolThicknessSlider() override = default;

    xoj::util::WidgetSPtr createItem(bool horizontal) override;

    /// Slider position from a thickness in page points. Logarithmic, so small values stay adjustable.
    static double scaleFunction(double thickness);
    static double scaleInverseFunction(double sliderValue);

    static std::string formatThickness(double thickness);

protected:
    static constexpr bool DISPLAY_VALUE = false;

    std::string getToolDisplayName() const override;
    GtkWidget* getNewToolIcon() const override;

private:
    std::string iconName;
    std::string tooltip;
    std::string displayName;

    template <class FinalSliderType>
    friend class SliderItemCreationHelper;
};
