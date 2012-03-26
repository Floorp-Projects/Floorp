/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import android.graphics.RectF;
import org.mozilla.gecko.FloatUtils;

final class DisplayPortCalculator {
    private static final String LOGTAG = "GeckoDisplayPortCalculator";

    private static final int DEFAULT_DISPLAY_PORT_MARGIN = 300;

    /* If the visible rect is within the danger zone (measured in pixels from each edge of a tile),
     * we start aggressively redrawing to minimize checkerboarding. */
    private static final int DANGER_ZONE_X = 75;
    private static final int DANGER_ZONE_Y = 150;

    static DisplayPortMetrics calculate(ImmutableViewportMetrics metrics) {
        float desiredXMargins = 2 * DEFAULT_DISPLAY_PORT_MARGIN;
        float desiredYMargins = 2 * DEFAULT_DISPLAY_PORT_MARGIN;

        // we need to avoid having a display port that is larger than the page, or we will end up
        // painting things outside the page bounds (bug 729169). we simultaneously need to make
        // the display port as large as possible so that we redraw less.

        // figure out how much of the desired buffer amount we can actually use on the horizontal axis
        float xBufferAmount = Math.min(desiredXMargins, metrics.pageSizeWidth - metrics.getWidth());
        // if we reduced the buffer amount on the horizontal axis, we should take that saved memory and
        // use it on the vertical axis
        float savedPixels = (desiredXMargins - xBufferAmount) * (metrics.getHeight() + desiredYMargins);
        float extraYAmount = (float)Math.floor(savedPixels / (metrics.getWidth() + xBufferAmount));
        float yBufferAmount = Math.min(desiredYMargins + extraYAmount, metrics.pageSizeHeight - metrics.getHeight());
        // and the reverse - if we shrunk the buffer on the vertical axis we can add it to the horizontal
        if (xBufferAmount == desiredXMargins && yBufferAmount < desiredYMargins) {
            savedPixels = (desiredYMargins - yBufferAmount) * (metrics.getWidth() + xBufferAmount);
            float extraXAmount = (float)Math.floor(savedPixels / (metrics.getHeight() + yBufferAmount));
            xBufferAmount = Math.min(xBufferAmount + extraXAmount, metrics.pageSizeWidth - metrics.getWidth());
        }

        // and now calculate the display port margins based on how much buffer we've decided to use and
        // the page bounds, ensuring we use all of the available buffer amounts on one side or the other
        // on any given axis. (i.e. if we're scrolled to the top of the page, the vertical buffer is
        // entirely below the visible viewport, but if we're halfway down the page, the vertical buffer
        // is split).
        float leftMargin = Math.min(DEFAULT_DISPLAY_PORT_MARGIN, metrics.viewportRectLeft);
        float rightMargin = Math.min(DEFAULT_DISPLAY_PORT_MARGIN, metrics.pageSizeWidth - (metrics.viewportRectLeft + metrics.getWidth()));
        if (leftMargin < DEFAULT_DISPLAY_PORT_MARGIN) {
            rightMargin = xBufferAmount - leftMargin;
        } else if (rightMargin < DEFAULT_DISPLAY_PORT_MARGIN) {
            leftMargin = xBufferAmount - rightMargin;
        } else if (!FloatUtils.fuzzyEquals(leftMargin + rightMargin, xBufferAmount)) {
            float delta = xBufferAmount - leftMargin - rightMargin;
            leftMargin += delta / 2;
            rightMargin += delta / 2;
        }

        float topMargin = Math.min(DEFAULT_DISPLAY_PORT_MARGIN, metrics.viewportRectTop);
        float bottomMargin = Math.min(DEFAULT_DISPLAY_PORT_MARGIN, metrics.pageSizeHeight - (metrics.viewportRectTop + metrics.getHeight()));
        if (topMargin < DEFAULT_DISPLAY_PORT_MARGIN) {
            bottomMargin = yBufferAmount - topMargin;
        } else if (bottomMargin < DEFAULT_DISPLAY_PORT_MARGIN) {
            topMargin = yBufferAmount - bottomMargin;
        } else if (!FloatUtils.fuzzyEquals(topMargin + bottomMargin, yBufferAmount)) {
            float delta = yBufferAmount - topMargin - bottomMargin;
            topMargin += delta / 2;
            bottomMargin += delta / 2;
        }

        // note that unless the viewport size changes, or the page dimensions change (either because of
        // content changes or zooming), the size of the display port should remain constant. this
        // is intentional to avoid re-creating textures and all sorts of other reallocations in the
        // draw and composition code.
        return new DisplayPortMetrics(metrics.viewportRectLeft - leftMargin,
                metrics.viewportRectTop - topMargin,
                metrics.viewportRectRight + rightMargin,
                metrics.viewportRectBottom + bottomMargin,
                metrics.zoomFactor);
    }

    // Returns true if a checkerboard is about to be visible.
    static boolean aboutToCheckerboard(ImmutableViewportMetrics metrics, DisplayPortMetrics displayPort) {
        if (displayPort == null) {
            return true;
        }

        // Increase the size of the viewport (and clamp to page boundaries), and
        // intersect it with the tile's displayport to determine whether we're
        // close to checkerboarding.
        FloatSize pageSize = metrics.getPageSize();
        RectF adjustedViewport = RectUtils.expand(metrics.getViewport(), DANGER_ZONE_X, DANGER_ZONE_Y);
        if (adjustedViewport.top < 0) adjustedViewport.top = 0;
        if (adjustedViewport.left < 0) adjustedViewport.left = 0;
        if (adjustedViewport.right > pageSize.width) adjustedViewport.right = pageSize.width;
        if (adjustedViewport.bottom > pageSize.height) adjustedViewport.bottom = pageSize.height;

        return !displayPort.contains(adjustedViewport);
    }
}
