#pragma once

#include <optional>

#include "util/Recolor.h"

struct RecolorParameters {
    bool recolorizeMainView = false;
    bool recolorizeSidebarMiniatures = false;
    /// Recolor the PDF background only. Drawings keep their original colors.
    bool recolorizePdf = false;
    Recolor recolor;

    bool operator==(const RecolorParameters& other) const {
        return recolorizeMainView == other.recolorizeMainView &&
               recolorizeSidebarMiniatures == other.recolorizeSidebarMiniatures &&
               recolorizePdf == other.recolorizePdf && recolor == other.recolor;
    }

    bool operator!=(const RecolorParameters& other) const { return !(*this == other); }

    /**
     * Recolor applied while rendering a PDF into its cache.
     * Empty when PDF recoloring is off, or when a later full-surface recolor already covers the PDF
     * (so the PDF must stay unmapped to avoid applying the transform twice).
     */
    std::optional<Recolor> cachedPdfRecolor(bool coveringRecolorActive) const {
        if (!recolorizePdf || coveringRecolorActive) {
            return std::nullopt;
        }
        return recolor;
    }
};
