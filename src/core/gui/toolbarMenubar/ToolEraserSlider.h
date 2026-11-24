/*
 * Xournal++
 *
 * Toolbar slider for the eraser radius.
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string>

#include "gui/IconNameHelper.h"

#include "AbstractSliderItem.h"

class ActionDatabase;

class ToolEraserSlider: public AbstractSliderItem {
public:
    ToolEraserSlider(std::string id, IconNameHelper iconNameHelper, ActionDatabase& db);
    ~ToolEraserSlider() override = default;

    xoj::util::WidgetSPtr createItem(bool horizontal) override;

    /// Slider position from a radius in page points. Logarithmic, so small radii stay adjustable.
    static double scaleFunction(double radius);
    static double scaleInverseFunction(double sliderValue);

    static std::string formatRadius(double radius);

protected:
    static constexpr bool DISPLAY_VALUE = false;

    std::string getToolDisplayName() const override;
    GtkWidget* getNewToolIcon() const override;

private:
    std::string iconName;

    template <class FinalSliderType>
    friend class SliderItemCreationHelper;
};
