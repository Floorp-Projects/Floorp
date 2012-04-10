/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import android.graphics.PointF;
import android.graphics.RectF;
import android.util.Log;
import org.mozilla.gecko.FloatUtils;
import org.mozilla.gecko.GeckoAppShell;

final class DisplayPortCalculator {
    private static final String LOGTAG = "GeckoDisplayPortCalculator";
    private static final PointF ZERO_VELOCITY = new PointF(0, 0);

    private static DisplayPortStrategy sStrategy = new VelocityBiasStrategy();

    static DisplayPortMetrics calculate(ImmutableViewportMetrics metrics, PointF velocity) {
        return sStrategy.calculate(metrics, (velocity == null ? ZERO_VELOCITY : velocity));
    }

    static boolean aboutToCheckerboard(ImmutableViewportMetrics metrics, PointF velocity, DisplayPortMetrics displayPort) {
        if (displayPort == null) {
            return true;
        }
        return sStrategy.aboutToCheckerboard(metrics, (velocity == null ? ZERO_VELOCITY : velocity), displayPort);
    }

    /**
     * Set the active strategy to use.
     * See the gfx.displayport.strategy pref in mobile/android/app/mobile.js to see the
     * mapping between ints and strategies.
     */
    static void setStrategy(int strategy) {
        switch (strategy) {
            case 0:
            default:
                sStrategy = new FixedMarginStrategy();
                break;
            case 1:
                sStrategy = new VelocityBiasStrategy();
                break;
            case 2:
                sStrategy = new DynamicResolutionStrategy();
                break;
            case 3:
                sStrategy = new NoMarginStrategy();
                break;
        }
        Log.i(LOGTAG, "Set strategy " + sStrategy.getClass().getName());
    }

    private interface DisplayPortStrategy {
        /** Calculates a displayport given a viewport and panning velocity. */
        public DisplayPortMetrics calculate(ImmutableViewportMetrics metrics, PointF velocity);
        /** Returns true if a checkerboard is about to be visible and we should not throttle drawing. */
        public boolean aboutToCheckerboard(ImmutableViewportMetrics metrics, PointF velocity, DisplayPortMetrics displayPort);
    }

    /**
     * Return the dimensions for a rect that has area (width*height) that does not exceed the page size in the
     * given metrics object. The area in the returned FloatSize may be less than width*height if the page is
     * small, but it will never be larger than width*height.
     * Note that this process may change the relative aspect ratio of the given dimensions.
     */
    private static FloatSize reshapeForPage(float width, float height, ImmutableViewportMetrics metrics) {
        // figure out how much of the desired buffer amount we can actually use on the horizontal axis
        float usableWidth = Math.min(width, metrics.pageSizeWidth);
        // if we reduced the buffer amount on the horizontal axis, we should take that saved memory and
        // use it on the vertical axis
        float extraUsableHeight = (float)Math.floor(((width - usableWidth) * height) / usableWidth);
        float usableHeight = Math.min(height + extraUsableHeight, metrics.pageSizeHeight);
        if (usableHeight < height && usableWidth == width) {
            // and the reverse - if we shrunk the buffer on the vertical axis we can add it to the horizontal
            float extraUsableWidth = (float)Math.floor(((height - usableHeight) * width) / usableHeight);
            usableWidth = Math.min(width + extraUsableWidth, metrics.pageSizeWidth);
        }
        return new FloatSize(usableWidth, usableHeight);
    }

    /**
     * Expand the given rect in all directions by a "danger zone". The size of the danger zone on an axis
     * is the size of the view on that axis multiplied by the given multiplier. The expanded rect is then
     * clamped to page bounds and returned.
     */
    private static RectF expandByDangerZone(RectF rect, float dangerZoneXMultiplier, float dangerZoneYMultiplier, ImmutableViewportMetrics metrics) {
        // calculate the danger zone amounts in pixels
        float dangerZoneX = metrics.getWidth() * dangerZoneXMultiplier;
        float dangerZoneY = metrics.getHeight() * dangerZoneYMultiplier;
        rect = RectUtils.expand(rect, dangerZoneX, dangerZoneY);
        // clamp to page bounds
        if (rect.top < 0) rect.top = 0;
        if (rect.left < 0) rect.left = 0;
        if (rect.right > metrics.pageSizeWidth) rect.right = metrics.pageSizeWidth;
        if (rect.bottom > metrics.pageSizeHeight) rect.bottom = metrics.pageSizeHeight;
        return rect;
    }

    /**
     * Adjust the given margins so if they are applied on the viewport in the metrics, the resulting rect
     * does not exceed the page bounds. This code will maintain the total margin amount for a given axis;
     * it assumes that margins.left + metrics.getWidth() + margins.right is less than or equal to
     * metrics.pageSizeWidth; and the same for the y axis.
     */
    private static RectF shiftMarginsForPageBounds(RectF margins, ImmutableViewportMetrics metrics) {
        // check how much we're overflowing in each direction. note that at most one of leftOverflow
        // and rightOverflow can be greater than zero, and at most one of topOverflow and bottomOverflow
        // can be greater than zero, because of the assumption described in the method javadoc.
        float leftOverflow = margins.left - metrics.viewportRectLeft;
        float rightOverflow = margins.right - (metrics.pageSizeWidth - metrics.viewportRectRight);
        float topOverflow = margins.top - metrics.viewportRectTop;
        float bottomOverflow = margins.bottom - (metrics.pageSizeHeight - metrics.viewportRectBottom);

        // if the margins overflow the page bounds, shift them to other side on the same axis
        if (leftOverflow > 0) {
            margins.left -= leftOverflow;
            margins.right += leftOverflow;
        } else if (rightOverflow > 0) {
            margins.right -= rightOverflow;
            margins.left += rightOverflow;
        }
        if (topOverflow > 0) {
            margins.top -= topOverflow;
            margins.bottom += topOverflow;
        } else if (bottomOverflow > 0) {
            margins.bottom -= bottomOverflow;
            margins.top += bottomOverflow;
        }
        return margins;
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
        // The length of each axis of the display port will be the corresponding view length
        // multiplied by this factor.
        private static final float SIZE_MULTIPLIER = 1.5f;

        // If the visible rect is within the danger zone (measured as a fraction of the view size
        // from the edge of the displayport) we start redrawing to minimize checkerboarding.
        private static final float DANGER_ZONE_X_MULTIPLIER = 0.10f;
        private static final float DANGER_ZONE_Y_MULTIPLIER = 0.20f;

        public DisplayPortMetrics calculate(ImmutableViewportMetrics metrics, PointF velocity) {
            float displayPortWidth = metrics.getWidth() * SIZE_MULTIPLIER;
            float displayPortHeight = metrics.getHeight() * SIZE_MULTIPLIER;

            // we need to avoid having a display port that is larger than the page, or we will end up
            // painting things outside the page bounds (bug 729169). we simultaneously need to make
            // the display port as large as possible so that we redraw less. reshape the display
            // port dimensions to accomplish this.
            FloatSize usableSize = reshapeForPage(displayPortWidth, displayPortHeight, metrics);
            float horizontalBuffer = usableSize.width - metrics.getWidth();
            float verticalBuffer = usableSize.height - metrics.getHeight();

            // and now calculate the display port margins based on how much buffer we've decided to use and
            // the page bounds, ensuring we use all of the available buffer amounts on one side or the other
            // on any given axis. (i.e. if we're scrolled to the top of the page, the vertical buffer is
            // entirely below the visible viewport, but if we're halfway down the page, the vertical buffer
            // is split).
            RectF margins = new RectF();
            margins.left = horizontalBuffer / 2.0f;
            margins.right = horizontalBuffer - margins.left;
            margins.top = verticalBuffer / 2.0f;
            margins.bottom = verticalBuffer - margins.top;
            margins = shiftMarginsForPageBounds(margins, metrics);

            // note that unless the viewport size changes, or the page dimensions change (either because of
            // content changes or zooming), the size of the display port should remain constant. this
            // is intentional to avoid re-creating textures and all sorts of other reallocations in the
            // draw and composition code.
            return new DisplayPortMetrics(metrics.viewportRectLeft - margins.left,
                    metrics.viewportRectTop - margins.top,
                    metrics.viewportRectRight + margins.right,
                    metrics.viewportRectBottom + margins.bottom,
                    metrics.zoomFactor);
        }

        public boolean aboutToCheckerboard(ImmutableViewportMetrics metrics, PointF velocity, DisplayPortMetrics displayPort) {
            // Increase the size of the viewport based on the danger zone multiplier (and clamp to page
            // boundaries), and intersect it with the current displayport to determine whether we're
            // close to checkerboarding.
            RectF adjustedViewport = expandByDangerZone(metrics.getViewport(), DANGER_ZONE_X_MULTIPLIER, DANGER_ZONE_Y_MULTIPLIER, metrics);
            return !displayPort.contains(adjustedViewport);
        }
    }

    /**
     * This class implements the variation with a small fixed-size margin with velocity bias.
     * In this variation, the default margins are pretty small relative to the view size, but
     * they are affected by the panning velocity. Specifically, if we are panning on one axis,
     * we remove the margins on the other axis because we are likely axis-locked. Also once
     * we are panning in one direction above a certain threshold velocity, we shift the buffer
     * so that it is almost entirely in the direction of the pan, with a little bit in the
     * reverse direction.
     */
    private static class VelocityBiasStrategy implements DisplayPortStrategy {
        // The length of each axis of the display port will be the corresponding view length
        // multiplied by this factor.
        private static final float SIZE_MULTIPLIER = 1.5f;
        // The velocity above which we apply the velocity bias
        private static final float VELOCITY_THRESHOLD = GeckoAppShell.getDpi() / 32f;
        // How much of the buffer to keep in the reverse direction of the velocity
        private static final float REVERSE_BUFFER = 0.2f;

        public DisplayPortMetrics calculate(ImmutableViewportMetrics metrics, PointF velocity) {
            float displayPortWidth = metrics.getWidth() * SIZE_MULTIPLIER;
            float displayPortHeight = metrics.getHeight() * SIZE_MULTIPLIER;

            // but if we're panning on one axis, set the margins for the other axis to zero since we are likely
            // axis locked and won't be displaying that extra area.
            if (Math.abs(velocity.x) > VELOCITY_THRESHOLD && FloatUtils.fuzzyEquals(velocity.y, 0)) {
                displayPortHeight = metrics.getHeight();
            } else if (Math.abs(velocity.y) > VELOCITY_THRESHOLD && FloatUtils.fuzzyEquals(velocity.x, 0)) {
                displayPortWidth = metrics.getWidth();
            }

            // we need to avoid having a display port that is larger than the page, or we will end up
            // painting things outside the page bounds (bug 729169).
            displayPortWidth = Math.min(displayPortWidth, metrics.pageSizeWidth);
            displayPortHeight = Math.min(displayPortHeight, metrics.pageSizeHeight);
            float horizontalBuffer = displayPortWidth - metrics.getWidth();
            float verticalBuffer = displayPortHeight - metrics.getHeight();

            // if we're panning above the VELOCITY_THRESHOLD on an axis, apply the margin so that it
            // is entirely in the direction of panning. Otherwise, split the margin evenly on both sides of
            // the display port.
            RectF margins = new RectF();
            if (velocity.x > VELOCITY_THRESHOLD) {
                margins.left = horizontalBuffer * REVERSE_BUFFER;
            } else if (velocity.x < -VELOCITY_THRESHOLD) {
                margins.left = horizontalBuffer * (1.0f - REVERSE_BUFFER);
            } else {
                margins.left = horizontalBuffer / 2.0f;
            }
            margins.right = horizontalBuffer - margins.left;

            if (velocity.y > VELOCITY_THRESHOLD) {
                margins.top = verticalBuffer * REVERSE_BUFFER;
            } else if (velocity.y < -VELOCITY_THRESHOLD) {
                margins.top = verticalBuffer * (1.0f - REVERSE_BUFFER);
            } else {
                margins.top = verticalBuffer / 2.0f;
            }
            margins.bottom = verticalBuffer - margins.top;

            // and finally shift the margins to account for page bounds
            margins = shiftMarginsForPageBounds(margins, metrics);

            return new DisplayPortMetrics(metrics.viewportRectLeft - margins.left,
                    metrics.viewportRectTop - margins.top,
                    metrics.viewportRectRight + margins.right,
                    metrics.viewportRectBottom + margins.bottom,
                    metrics.zoomFactor);
        }

        public boolean aboutToCheckerboard(ImmutableViewportMetrics metrics, PointF velocity, DisplayPortMetrics displayPort) {
            // Since we have such a small margin, we want to be drawing more aggressively. At the start of a
            // pan the velocity is going to be large so we're almost certainly going to go into checkerboard
            // on every frame, so drawing all the time seems like the right thing. At the end of the pan we
            // want to re-center the displayport and draw stuff on all sides, so again we don't want to throttle
            // there. When we're not panning we're not drawing anyway so it doesn't make a difference there.
            return true;
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
        // greater than (SIZE_MULTIPLIER - 1.0f), then at zero velocity we will always be in the
        // danger zone, and thus will be constantly drawing.
        private static final float PREDICTION_VELOCITY_MULTIPLIER = 30.0f;
        private static final float DANGER_ZONE_MULTIPLIER = 0.20f; // must be less than (SIZE_MULTIPLIER - 1.0f)

        public DisplayPortMetrics calculate(ImmutableViewportMetrics metrics, PointF velocity) {
            float displayPortWidth = metrics.getWidth() * SIZE_MULTIPLIER;
            float displayPortHeight = metrics.getHeight() * SIZE_MULTIPLIER;

            // for resolution calculation purposes, we need to know what the adjusted display port dimensions
            // would be if we had zero velocity, so calculate that here before we increase the display port
            // based on velocity.
            FloatSize reshapedSize = reshapeForPage(displayPortWidth, displayPortHeight, metrics);

            // increase displayPortWidth and displayPortHeight based on the velocity, but maintaining their
            // relative aspect ratio.
            if (velocity.length() > VELOCITY_EXPANSION_THRESHOLD) {
                float velocityFactor = Math.max(Math.abs(velocity.x) / displayPortWidth,
                                                Math.abs(velocity.y) / displayPortHeight);
                velocityFactor *= VELOCITY_MULTIPLIER;

                displayPortWidth += (displayPortWidth * velocityFactor);
                displayPortHeight += (displayPortHeight * velocityFactor);
            }

            // at this point, displayPortWidth and displayPortHeight are how much of the page (in device pixels)
            // we want to be rendered by Gecko. Note here "device pixels" is equivalent to CSS pixels multiplied
            // by metrics.zoomFactor

            // we need to avoid having a display port that is larger than the page, or we will end up
            // painting things outside the page bounds (bug 729169). we simultaneously need to make
            // the display port as large as possible so that we redraw less. reshape the display
            // port dimensions to accomplish this. this may change the aspect ratio of the display port,
            // but we are assuming that this is desirable because the advantages from pre-drawing will
            // outweigh the disadvantages from any buffer reallocations that might occur.
            FloatSize usableSize = reshapeForPage(displayPortWidth, displayPortHeight, metrics);
            float horizontalBuffer = usableSize.width - metrics.getWidth();
            float verticalBuffer = usableSize.height - metrics.getHeight();

            // at this point, horizontalBuffer and verticalBuffer are the dimensions of the buffer area we have.
            // the buffer area is the off-screen area that is part of the display port and will be pre-drawn in case
            // the user scrolls there. we now need to split the buffer area on each axis so that we know
            // what the exact margins on each side will be. first we split the buffer amount based on the direction
            // we're moving, so that we have a larger buffer in the direction of travel.
            RectF margins = new RectF();
            margins.left = splitBufferByVelocity(horizontalBuffer, velocity.x);
            margins.right = horizontalBuffer - margins.left;
            margins.top = splitBufferByVelocity(verticalBuffer, velocity.y);
            margins.bottom = verticalBuffer - margins.top;

            // then, we account for running into the page bounds - so that if we hit the top of the page, we need
            // to drop the top margin and move that amount to the bottom margin.
            margins = shiftMarginsForPageBounds(margins, metrics);

            // finally, we calculate the resolution we want to render the display port area at. We do this
            // so that as we expand the display port area (because of velocity), we reduce the resolution of
            // the painted area so as to maintain the size of the buffer Gecko is painting into. we calculate
            // the reduction in resolution by comparing the display port size with and without the velocity
            // changes applied.
            // this effectively means that as we pan faster and faster, the display port grows, but we paint
            // at lower resolutions. this paints more area to reduce checkerboard at the cost of increasing
            // compositor-scaling and blurriness. Once we stop panning, the blurriness must be entirely gone.
            // Note that usable* could be less than base* if we are pinch-zoomed out into overscroll, so we
            // clamp it to make sure this doesn't increase our display resolution past metrics.zoomFactor.
            float scaleFactor = Math.min(reshapedSize.width / usableSize.width, reshapedSize.height / usableSize.height);
            float displayResolution = metrics.zoomFactor * Math.min(1.0f, scaleFactor);

            DisplayPortMetrics dpMetrics = new DisplayPortMetrics(
                metrics.viewportRectLeft - margins.left,
                metrics.viewportRectTop - margins.top,
                metrics.viewportRectRight + margins.right,
                metrics.viewportRectBottom + margins.bottom,
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
            // Expand the viewport based on our velocity (and clamp it to page boundaries).
            // Then intersect it with the last-requested displayport to determine whether we're
            // close to checkerboarding.

            RectF predictedViewport = metrics.getViewport();

            // first we expand the viewport in the direction we're moving based on some
            // multiple of the current velocity.
            if (velocity.length() > 0) {
                if (velocity.x < 0) {
                    predictedViewport.left += velocity.x * PREDICTION_VELOCITY_MULTIPLIER;
                } else if (velocity.x > 0) {
                    predictedViewport.right += velocity.x * PREDICTION_VELOCITY_MULTIPLIER;
                }

                if (velocity.y < 0) {
                    predictedViewport.top += velocity.y * PREDICTION_VELOCITY_MULTIPLIER;
                } else if (velocity.y > 0) {
                    predictedViewport.bottom += velocity.y * PREDICTION_VELOCITY_MULTIPLIER;
                }
            }

            // then we expand the viewport evenly in all directions just to have an extra
            // safety zone. this also clamps it to page bounds.
            predictedViewport = expandByDangerZone(predictedViewport, DANGER_ZONE_MULTIPLIER, DANGER_ZONE_MULTIPLIER, metrics);
            return !displayPort.contains(predictedViewport);
        }
    }
}
