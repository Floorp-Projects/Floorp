/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import android.graphics.PointF;
import android.graphics.RectF;
import org.mozilla.gecko.FloatUtils;
import org.mozilla.gecko.GeckoAppShell;

final class DisplayPortCalculator {
    private static final String LOGTAG = "GeckoDisplayPortCalculator";

    private static DisplayPortStrategy sStrategy = new FixedMarginStrategy();

    static DisplayPortMetrics calculate(ImmutableViewportMetrics metrics, PointF velocity) {
        return sStrategy.calculate(metrics, velocity);
    }

    static boolean aboutToCheckerboard(ImmutableViewportMetrics metrics, PointF velocity, DisplayPortMetrics displayPort) {
        return sStrategy.aboutToCheckerboard(metrics, velocity, displayPort);
    }

    private interface DisplayPortStrategy {
        /** Calculates a displayport given a viewport and panning velocity. */
        public DisplayPortMetrics calculate(ImmutableViewportMetrics metrics, PointF velocity);
        /** Returns true if a checkerboard is about to be visible and we should not throttle drawing. */
        public boolean aboutToCheckerboard(ImmutableViewportMetrics metrics, PointF velocity, DisplayPortMetrics displayPort);
    }

    /**
     * This class implements the variation where we basically don't bother with a a display port.
     */
    private static class NoMarginStrategy implements DisplayPortStrategy {
        public DisplayPortMetrics calculate(ImmutableViewportMetrics metrics, PointF velocity) {
            return new DisplayPortMetrics(metrics.viewportRectLeft,
                    metrics.viewportRectTop,
                    metrics.viewportRectRight,
                    metrics.viewportRectBottom,
                    metrics.zoomFactor);
        }

        public boolean aboutToCheckerboard(ImmutableViewportMetrics metrics, PointF velocity, DisplayPortMetrics displayPort) {
            return true;
        }
    }

    /**
     * This class implements the variation where we use a fixed-size margin on the display port.
     * The margin is always 300 pixels in all directions, except when we are (a) approaching a page
     * boundary, and/or (b) if we are limited by the page size. In these cases we try to maintain
     * the area of the display port by (a) shifting the buffer to the other side on the same axis,
     * and/or (b) increasing the buffer on the other axis to compensate for the reduced buffer on
     * one axis.
     */
    private static class FixedMarginStrategy implements DisplayPortStrategy {
        private static final int DEFAULT_DISPLAY_PORT_MARGIN = 300;

        /* If the visible rect is within the danger zone (measured in pixels from each edge of a tile),
         * we start aggressively redrawing to minimize checkerboarding. */
        private static final int DANGER_ZONE_X = 75;
        private static final int DANGER_ZONE_Y = 150;

        public DisplayPortMetrics calculate(ImmutableViewportMetrics metrics, PointF velocity) {
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

        public boolean aboutToCheckerboard(ImmutableViewportMetrics metrics, PointF velocity, DisplayPortMetrics displayPort) {
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

    /**
     * This class implements the variation where we draw more of the page at low resolution while panning.
     * In this variation, as we pan faster, we increase the page area we are drawing, but reduce the draw
     * resolution to compensate. This results in the same device-pixel area drawn; the compositor then
     * scales this up to the viewport zoom level. This results in a large area of the page drawn but it
     * looks blurry. The assumption is that drawing extra that we never display is better than checkerboarding,
     * where we draw less but never even show it on the screen.
     */
    private static class DynamicResolutionStrategy implements DisplayPortStrategy {
        // The length of each axis of the display port will be the corresponding view length
        // multiplied by this factor.
        private static final float SIZE_MULTIPLIER = 1.5f;

        // The velocity above which we start zooming out the display port to keep up
        // with the panning.
        private static final float VELOCITY_EXPANSION_THRESHOLD = GeckoAppShell.getDpi() / 16f;

        // How much we increase the display port based on velocity. Assuming no friction and
        // splitting (see below), this should be be the number of frames (@60fps) between us
        // calculating the display port and the draw of the *next* display port getting composited
        // and displayed on the screen. This is because the timeline looks like this:
        //      Java: pan pan pan pan pan pan ! pan pan pan pan pan pan !
        //     Gecko:   \-> draw -> composite /   \-> draw -> composite /
        // The display port calculated on the first "pan" gets composited to the screen at the
        // first exclamation mark, and remains on the screen until the second exclamation mark.
        // In order to avoid checkerboarding, that display port must be able to contain all of
        // the panning until the second exclamation mark, which encompasses two entire draw/composite
        // cycles.
        // If we take into account friction, our velocity multiplier should be reduced as the
        // amount of pan will decrease each time. If we take into account display port splitting,
        // it should be increased as the splitting means some of the display port will be used to
        // draw in the opposite direction of the velocity. For now I'm assuming these two cancel
        // each other out.
        private static final float VELOCITY_MULTIPLIER = 60.0f;

        // The following constants adjust how biased the display port is in the direction of panning.
        // When panning fast (above the FAST_THRESHOLD) we use the fast split factor to split the
        // display port "buffer" area, otherwise we use the slow split factor. This is based on the
        // assumption that if the user is panning fast, they are less likely to reverse directions
        // and go backwards, so we should spend more of our display port buffer in the direction of
        // panning.
        private static final float VELOCITY_FAST_THRESHOLD = VELOCITY_EXPANSION_THRESHOLD * 2.0f;
        private static final float FAST_SPLIT_FACTOR = 0.95f;
        private static final float SLOW_SPLIT_FACTOR = 0.8f;

        // The following constants are used for viewport prediction; we use them to estimate where
        // the viewport will be soon and whether or not we should trigger a draw right now. "soon"
        // in the previous sentence really refers to the amount of time it would take to draw and
        // composite from the point at which we do the calculation, and that is not really a known
        // quantity. The velocity multiplier is how much we multiply the velocity by; it has the
        // same caveats as the VELOCITY_MULTIPLIER above except that it only needs to take into account
        // one draw/composite cycle instead of two. The danger zone multiplier is a multiplier of the
        // viewport size that we use as an extra "danger zone" around the viewport; if this danger
        // zone falls outside the display port then we are approaching the point at which we will
        // checkerboard, and hence should start drawing. Note that if DANGER_ZONE_MULTIPLIER is
        // greater than (SIZE_MULTIPLIER - 1.0f) / 2, then at zero velocity we will always be in the
        // danger zone, and thus will be constantly drawing.
        private static final float PREDICTION_VELOCITY_MULTIPLIER = 30.0f;
        private static final float DANGER_ZONE_MULTIPLIER = 0.10f; // must be less than (SIZE_MULTIPLIER - 1.0f) / 2

        public DisplayPortMetrics calculate(ImmutableViewportMetrics metrics, PointF velocity) {
            float baseWidth = metrics.getWidth() * SIZE_MULTIPLIER;
            float baseHeight = metrics.getHeight() * SIZE_MULTIPLIER;

            float width = baseWidth;
            float height = baseHeight;
            if (velocity != null && velocity.length() > VELOCITY_EXPANSION_THRESHOLD) {
                // increase width and height based on the velocity, but maintaining aspect ratio.
                float velocityFactor = Math.max(Math.abs(velocity.x) / width,
                                                Math.abs(velocity.y) / height);
                velocityFactor *= VELOCITY_MULTIPLIER;

                width += (width * velocityFactor);
                height += (height * velocityFactor);
            }

            // at this point, width and height are how much of the page (in device pixels) we want to
            // be rendered by Gecko. Note here "device pixels" is equivalent to CSS pixels multiplied
            // by metrics.zoomFactor

            // we need to avoid having a display port that is larger than the page, or we will end up
            // painting things outside the page bounds (bug 729169). we simultaneously need to make
            // the display port as large as possible so that we redraw less.
            //
            // so, first we figure out how much of the desired amount we can actually use on the horizontal axis,
            // and transfer any unused amount to the vertical axis. Then we see if we have unused amounts on
            // the vertical axis and transfer them back (if possible) to the horizontal axis.
            //
            // Note that this process may throw the aspect ratio out of whack, possibly requiring various buffers
            // in Gecko and/or the compositor to be reallocated. Therefore it is not strictly desirable, but for
            // now we assume that the benefits outweigh the disadvantages.
            float usableWidth = Math.min(width, metrics.pageSizeWidth);
            float extraUsableHeight = ((width - usableWidth) * height) / usableWidth;
            float usableHeight = Math.min(height + extraUsableHeight, metrics.pageSizeHeight);
            if (usableHeight < height && usableWidth == width) {
                float extraUsableWidth = ((height - usableHeight) * width) / usableHeight;
                usableWidth = Math.min(width + extraUsableWidth, metrics.pageSizeWidth);
            }

            // now that we know how large the display port is, we allocate the area to the four sides as margins.
            // this ensures we use all of the available display port area on one side or the other of any given axis
            // in a desirable manner.

            float horizontalBuffer = usableWidth - metrics.getWidth();
            float verticalBuffer = usableHeight - metrics.getHeight();
            // first we split the buffer amount based on the direction we're moving, so that we have a larger buffer
            // in the direction of travel.
            float leftMargin = splitBufferByVelocity(horizontalBuffer, velocity.x);
            float rightMargin = horizontalBuffer - leftMargin;
            float topMargin = splitBufferByVelocity(verticalBuffer, velocity.y);
            float bottomMargin = verticalBuffer - topMargin;

            // then, we account for running into the page bounds - so that if we hit the top of the page, we need
            // to drop the top margin and move that amount to the bottom margin.
            if (metrics.viewportRectLeft - leftMargin < 0) {
                leftMargin = metrics.viewportRectLeft;
                rightMargin = horizontalBuffer - leftMargin;
            } else if (metrics.viewportRectRight + rightMargin > metrics.pageSizeWidth) {
                rightMargin = metrics.pageSizeWidth - metrics.viewportRectRight;
                leftMargin = horizontalBuffer - rightMargin;
            }
            if (metrics.viewportRectTop - topMargin < 0) {
                topMargin = metrics.viewportRectTop;
                bottomMargin = verticalBuffer - topMargin;
            } else if (metrics.viewportRectBottom + bottomMargin > metrics.pageSizeHeight) {
                bottomMargin = metrics.pageSizeHeight - metrics.viewportRectBottom;
                topMargin = verticalBuffer - bottomMargin;
            }

            // finally, we calculate the resolution we want to render the display port area at. We do this
            // so that as we expand the display port area (because of velocity), we reduce the resolution of
            // the painted area so as to maintain the size of the buffer Gecko is painting into (assuming no
            // aspect ratio changes as described above). if the velocity is zero, then the displayResolution
            // must be equal to metrics.zoomFactor.
            // this effectively means that as we pan faster and faster, the display port grows, but we paint
            // at lower resolutions. this paints more area to reduce checkerboard at the cost of increasing
            // compositor-scaling and blurriness. Once we stop panning, the blurriness must be entirely gone.
            // Note that usable* could be less than base* if we are pinch-zoomed out into overscroll, so we
            // clamp it to make sure this doesn't increase our display resolution past metrics.zoomFactor.
            float scaleFactor = Math.min(baseWidth / usableWidth, baseHeight / usableHeight);
            float displayResolution = metrics.zoomFactor * Math.min(1.0f, scaleFactor);

            DisplayPortMetrics dpMetrics = new DisplayPortMetrics(
                metrics.viewportRectLeft - leftMargin,
                metrics.viewportRectTop - topMargin,
                metrics.viewportRectRight + rightMargin,
                metrics.viewportRectBottom + bottomMargin,
                displayResolution);
            return dpMetrics;
        }

        /**
         * Split the given buffer amount into two based on the velocity.
         * Given an amount of total usable buffer on an axis, this will
         * return the amount that should be used on the left/top side of
         * the axis (the side which a negative velocity vector corresponds
         * to).
         */
        private float splitBufferByVelocity(float amount, float velocity) {
            // if no velocity, so split evenly
            if (FloatUtils.fuzzyEquals(velocity, 0)) {
                return amount / 2.0f;
            }
            // if we're moving quickly, assign more of the amount in that direction
            // since is is less likely that we will reverse direction immediately
            if (velocity < -VELOCITY_FAST_THRESHOLD) {
                return amount * FAST_SPLIT_FACTOR;
            }
            if (velocity > VELOCITY_FAST_THRESHOLD) {
                return amount * (1.0f - FAST_SPLIT_FACTOR);
            }
            // if we're moving slowly, then assign less of the amount in that direction
            if (velocity < 0) {
                return amount * SLOW_SPLIT_FACTOR;
            } else {
                return amount * (1.0f - SLOW_SPLIT_FACTOR);
            }
        }

        public boolean aboutToCheckerboard(ImmutableViewportMetrics metrics, PointF velocity, DisplayPortMetrics displayPort) {
            if (displayPort == null) {
                return true;
            }

            // Expand the viewport based on our velocity (and clamp it to page boundaries).
            // Then intersect it with the last-requested displayport to determine whether we're
            // close to checkerboarding.

            float left = metrics.viewportRectLeft;
            float right = metrics.viewportRectRight;
            float top = metrics.viewportRectTop;
            float bottom = metrics.viewportRectBottom;

            // first we expand the viewport in the direction we're moving based on some
            // multiple of the current velocity.
            if (velocity != null && velocity.length() > 0) {
                if (velocity.x < 0) {
                    left += velocity.x * PREDICTION_VELOCITY_MULTIPLIER;
                } else if (velocity.x > 0) {
                    right += velocity.x * PREDICTION_VELOCITY_MULTIPLIER;
                }

                if (velocity.y < 0) {
                    top += velocity.y * PREDICTION_VELOCITY_MULTIPLIER;
                } else if (velocity.y > 0) {
                    bottom += velocity.y * PREDICTION_VELOCITY_MULTIPLIER;
                }
            }

            // then we expand the viewport evenly in all directions just to have an extra
            // safety zone.
            float dangerZoneX = metrics.getWidth() * DANGER_ZONE_MULTIPLIER;
            float dangerZoneY = metrics.getHeight() * DANGER_ZONE_MULTIPLIER;
            left -= dangerZoneX;
            top -= dangerZoneY;
            right += dangerZoneX;
            bottom += dangerZoneY;

            // finally, we clamp the calculated viewport to the page bounds, since we will
            // never checkerboard outside of the page bounds.
            if (left < 0) left = 0;
            if (top < 0) top = 0;
            if (right > metrics.pageSizeWidth) right = metrics.pageSizeWidth;
            if (bottom > metrics.pageSizeHeight) bottom = metrics.pageSizeHeight;

            RectF predictedViewport = new RectF(left, top, right, bottom);
            return !displayPort.contains(predictedViewport);
        }
    }
}
