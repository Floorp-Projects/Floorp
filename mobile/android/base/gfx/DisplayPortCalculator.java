/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import java.util.Map;
import android.graphics.PointF;
import android.graphics.RectF;
import android.util.FloatMath;
import android.util.Log;
import org.json.JSONArray;
import org.mozilla.gecko.FloatUtils;
import org.mozilla.gecko.GeckoAppShell;

final class DisplayPortCalculator {
    private static final String LOGTAG = "GeckoDisplayPortCalculator";
    private static final PointF ZERO_VELOCITY = new PointF(0, 0);

    // Keep this in sync with the TILEDLAYERBUFFER_TILE_SIZE defined in gfx/layers/TiledLayerBuffer.h
    private static final int TILE_SIZE = 256;

    private static final String PREF_DISPLAYPORT_STRATEGY = "gfx.displayport.strategy";
    private static final String PREF_DISPLAYPORT_FM_MULTIPLIER = "gfx.displayport.strategy_fm.multiplier";
    private static final String PREF_DISPLAYPORT_FM_DANGER_X = "gfx.displayport.strategy_fm.danger_x";
    private static final String PREF_DISPLAYPORT_FM_DANGER_Y = "gfx.displayport.strategy_fm.danger_y";
    private static final String PREF_DISPLAYPORT_VB_MULTIPLIER = "gfx.displayport.strategy_vb.multiplier";
    private static final String PREF_DISPLAYPORT_VB_VELOCITY_THRESHOLD = "gfx.displayport.strategy_vb.threshold";
    private static final String PREF_DISPLAYPORT_VB_REVERSE_BUFFER = "gfx.displayport.strategy_vb.reverse_buffer";
    private static final String PREF_DISPLAYPORT_VB_DANGER_X_BASE = "gfx.displayport.strategy_vb.danger_x_base";
    private static final String PREF_DISPLAYPORT_VB_DANGER_Y_BASE = "gfx.displayport.strategy_vb.danger_y_base";
    private static final String PREF_DISPLAYPORT_VB_DANGER_X_INCR = "gfx.displayport.strategy_vb.danger_x_incr";
    private static final String PREF_DISPLAYPORT_VB_DANGER_Y_INCR = "gfx.displayport.strategy_vb.danger_y_incr";
    private static final String PREF_DISPLAYPORT_PB_VELOCITY_THRESHOLD = "gfx.displayport.strategy_pb.threshold";

    private static DisplayPortStrategy sStrategy = new VelocityBiasStrategy(null);

    static DisplayPortMetrics calculate(ImmutableViewportMetrics metrics, PointF velocity) {
        return sStrategy.calculate(metrics, (velocity == null ? ZERO_VELOCITY : velocity));
    }

    static boolean aboutToCheckerboard(ImmutableViewportMetrics metrics, PointF velocity, DisplayPortMetrics displayPort) {
        if (displayPort == null) {
            return true;
        }
        return sStrategy.aboutToCheckerboard(metrics, (velocity == null ? ZERO_VELOCITY : velocity), displayPort);
    }

    static boolean drawTimeUpdate(long millis, int pixels) {
        return sStrategy.drawTimeUpdate(millis, pixels);
    }

    static void resetPageState() {
        sStrategy.resetPageState();
    }

    static void addPrefNames(JSONArray prefs) {
        prefs.put(PREF_DISPLAYPORT_STRATEGY);
        prefs.put(PREF_DISPLAYPORT_FM_MULTIPLIER);
        prefs.put(PREF_DISPLAYPORT_FM_DANGER_X);
        prefs.put(PREF_DISPLAYPORT_FM_DANGER_Y);
        prefs.put(PREF_DISPLAYPORT_VB_MULTIPLIER);
        prefs.put(PREF_DISPLAYPORT_VB_VELOCITY_THRESHOLD);
        prefs.put(PREF_DISPLAYPORT_VB_REVERSE_BUFFER);
        prefs.put(PREF_DISPLAYPORT_VB_DANGER_X_BASE);
        prefs.put(PREF_DISPLAYPORT_VB_DANGER_Y_BASE);
        prefs.put(PREF_DISPLAYPORT_VB_DANGER_X_INCR);
        prefs.put(PREF_DISPLAYPORT_VB_DANGER_Y_INCR);
        prefs.put(PREF_DISPLAYPORT_PB_VELOCITY_THRESHOLD);
    }

    /**
     * Set the active strategy to use.
     * See the gfx.displayport.strategy pref in mobile/android/app/mobile.js to see the
     * mapping between ints and strategies.
     */
    static boolean setStrategy(Map<String, Integer> prefs) {
        Integer strategy = prefs.get(PREF_DISPLAYPORT_STRATEGY);
        if (strategy == null) {
            return false;
        }

        switch (strategy) {
            case 0:
                sStrategy = new FixedMarginStrategy(prefs);
                break;
            case 1:
                sStrategy = new VelocityBiasStrategy(prefs);
                break;
            case 2:
                sStrategy = new DynamicResolutionStrategy(prefs);
                break;
            case 3:
                sStrategy = new NoMarginStrategy(prefs);
                break;
            case 4:
                sStrategy = new PredictionBiasStrategy(prefs);
                break;
            default:
                Log.e(LOGTAG, "Invalid strategy index specified");
                return false;
        }
        Log.i(LOGTAG, "Set strategy " + sStrategy.toString());
        return true;
    }

    private static float getFloatPref(Map<String, Integer> prefs, String prefName, int defaultValue) {
        Integer value = (prefs == null ? null : prefs.get(prefName));
        return (float)(value == null || value < 0 ? defaultValue : value) / 1000f;
    }

    private static abstract class DisplayPortStrategy {
        /** Calculates a displayport given a viewport and panning velocity. */
        public abstract DisplayPortMetrics calculate(ImmutableViewportMetrics metrics, PointF velocity);
        /** Returns true if a checkerboard is about to be visible and we should not throttle drawing. */
        public abstract boolean aboutToCheckerboard(ImmutableViewportMetrics metrics, PointF velocity, DisplayPortMetrics displayPort);
        /** Notify the strategy of a new recorded draw time. Return false to turn off draw time recording. */
        public boolean drawTimeUpdate(long millis, int pixels) { return false; }
        /** Reset any page-specific state stored, as the page being displayed has changed. */
        public void resetPageState() {}
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
     * Expand the given margins such that when they are applied on the viewport, the resulting rect
     * does not have any partial tiles, except when it is clipped by the page bounds. This assumes
     * the tiles are TILE_SIZE by TILE_SIZE and start at the origin, such that there will always be
     * a tile at (0,0)-(TILE_SIZE,TILE_SIZE)).
     */
    private static DisplayPortMetrics getTileAlignedDisplayPortMetrics(RectF margins, float zoom, ImmutableViewportMetrics metrics) {
        float left = metrics.viewportRectLeft - margins.left;
        float top = metrics.viewportRectTop - margins.top;
        float right = metrics.viewportRectRight + margins.right;
        float bottom = metrics.viewportRectBottom + margins.bottom;
        left = Math.max(0.0f, TILE_SIZE * FloatMath.floor(left / TILE_SIZE));
        top = Math.max(0.0f, TILE_SIZE * FloatMath.floor(top / TILE_SIZE));
        right = Math.min(metrics.pageSizeWidth, TILE_SIZE * FloatMath.ceil(right / TILE_SIZE));
        bottom = Math.min(metrics.pageSizeHeight, TILE_SIZE * FloatMath.ceil(bottom / TILE_SIZE));
        return new DisplayPortMetrics(left, top, right, bottom, zoom);
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
     * Clamp the given rect to the page bounds and return it.
     */
    private static RectF clampToPageBounds(RectF rect, ImmutableViewportMetrics metrics) {
        rect.left = Math.max(rect.left, 0);
        rect.top = Math.max(rect.top, 0);
        rect.right = Math.min(rect.right, metrics.pageSizeWidth);
        rect.bottom = Math.min(rect.bottom, metrics.pageSizeHeight);
        return rect;
    }

    /**
     * This class implements the variation where we basically don't bother with a a display port.
     */
    private static class NoMarginStrategy extends DisplayPortStrategy {
        NoMarginStrategy(Map<String, Integer> prefs) {
            // no prefs in this strategy
        }

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

        @Override
        public String toString() {
            return "NoMarginStrategy";
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
    private static class FixedMarginStrategy extends DisplayPortStrategy {
        // The length of each axis of the display port will be the corresponding view length
        // multiplied by this factor.
        private final float SIZE_MULTIPLIER;

        // If the visible rect is within the danger zone (measured as a fraction of the view size
        // from the edge of the displayport) we start redrawing to minimize checkerboarding.
        private final float DANGER_ZONE_X_MULTIPLIER;
        private final float DANGER_ZONE_Y_MULTIPLIER;

        FixedMarginStrategy(Map<String, Integer> prefs) {
            SIZE_MULTIPLIER = getFloatPref(prefs, PREF_DISPLAYPORT_FM_MULTIPLIER, 2000);
            DANGER_ZONE_X_MULTIPLIER = getFloatPref(prefs, PREF_DISPLAYPORT_FM_DANGER_X, 100);
            DANGER_ZONE_Y_MULTIPLIER = getFloatPref(prefs, PREF_DISPLAYPORT_FM_DANGER_Y, 200);
        }

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

            return getTileAlignedDisplayPortMetrics(margins, metrics.zoomFactor, metrics);
        }

        public boolean aboutToCheckerboard(ImmutableViewportMetrics metrics, PointF velocity, DisplayPortMetrics displayPort) {
            // Increase the size of the viewport based on the danger zone multiplier (and clamp to page
            // boundaries), and intersect it with the current displayport to determine whether we're
            // close to checkerboarding.
            RectF adjustedViewport = expandByDangerZone(metrics.getViewport(), DANGER_ZONE_X_MULTIPLIER, DANGER_ZONE_Y_MULTIPLIER, metrics);
            return !displayPort.contains(adjustedViewport);
        }

        @Override
        public String toString() {
            return "FixedMarginStrategy mult=" + SIZE_MULTIPLIER + ", dangerX=" + DANGER_ZONE_X_MULTIPLIER + ", dangerY=" + DANGER_ZONE_Y_MULTIPLIER;
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
    private static class VelocityBiasStrategy extends DisplayPortStrategy {
        // The length of each axis of the display port will be the corresponding view length
        // multiplied by this factor.
        private final float SIZE_MULTIPLIER;
        // The velocity above which we apply the velocity bias
        private final float VELOCITY_THRESHOLD;
        // How much of the buffer to keep in the reverse direction of the velocity
        private final float REVERSE_BUFFER;
        // If the visible rect is within the danger zone we start redrawing to minimize
        // checkerboarding. the danger zone amount is a linear function of the form:
        //    viewportsize * (base + velocity * incr)
        // where base and incr are configurable values.
        private final float DANGER_ZONE_BASE_X_MULTIPLIER;
        private final float DANGER_ZONE_BASE_Y_MULTIPLIER;
        private final float DANGER_ZONE_INCR_X_MULTIPLIER;
        private final float DANGER_ZONE_INCR_Y_MULTIPLIER;

        VelocityBiasStrategy(Map<String, Integer> prefs) {
            SIZE_MULTIPLIER = getFloatPref(prefs, PREF_DISPLAYPORT_VB_MULTIPLIER, 2000);
            VELOCITY_THRESHOLD = GeckoAppShell.getDpi() * getFloatPref(prefs, PREF_DISPLAYPORT_VB_VELOCITY_THRESHOLD, 32);
            REVERSE_BUFFER = getFloatPref(prefs, PREF_DISPLAYPORT_VB_REVERSE_BUFFER, 200);
            DANGER_ZONE_BASE_X_MULTIPLIER = getFloatPref(prefs, PREF_DISPLAYPORT_VB_DANGER_X_BASE, 1000);
            DANGER_ZONE_BASE_Y_MULTIPLIER = getFloatPref(prefs, PREF_DISPLAYPORT_VB_DANGER_Y_BASE, 1000);
            DANGER_ZONE_INCR_X_MULTIPLIER = getFloatPref(prefs, PREF_DISPLAYPORT_VB_DANGER_X_INCR, 0);
            DANGER_ZONE_INCR_Y_MULTIPLIER = getFloatPref(prefs, PREF_DISPLAYPORT_VB_DANGER_Y_INCR, 0);
        }

        /**
         * Split the given amounts into margins based on the VELOCITY_THRESHOLD and REVERSE_BUFFER values.
         * If the velocity is above the VELOCITY_THRESHOLD on an axis, split the amount into REVERSE_BUFFER
         * and 1.0 - REVERSE_BUFFER fractions. The REVERSE_BUFFER fraction is set as the margin in the
         * direction opposite to the velocity, and the remaining fraction is set as the margin in the direction
         * of the velocity. If the velocity is lower than VELOCITY_THRESHOLD, split the amount evenly into the
         * two margins on that axis.
         */
        private RectF velocityBiasedMargins(float xAmount, float yAmount, PointF velocity) {
            RectF margins = new RectF();

            if (velocity.x > VELOCITY_THRESHOLD) {
                margins.left = xAmount * REVERSE_BUFFER;
            } else if (velocity.x < -VELOCITY_THRESHOLD) {
                margins.left = xAmount * (1.0f - REVERSE_BUFFER);
            } else {
                margins.left = xAmount / 2.0f;
            }
            margins.right = xAmount - margins.left;
    
            if (velocity.y > VELOCITY_THRESHOLD) {
                margins.top = yAmount * REVERSE_BUFFER;
            } else if (velocity.y < -VELOCITY_THRESHOLD) {
                margins.top = yAmount * (1.0f - REVERSE_BUFFER);
            } else {
                margins.top = yAmount / 2.0f;
            }
            margins.bottom = yAmount - margins.top;

            return margins;
        }

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

            // split the buffer amounts into margins based on velocity, and shift it to
            // take into account the page bounds
            RectF margins = velocityBiasedMargins(horizontalBuffer, verticalBuffer, velocity);
            margins = shiftMarginsForPageBounds(margins, metrics);

            return getTileAlignedDisplayPortMetrics(margins, metrics.zoomFactor, metrics);
        }

        public boolean aboutToCheckerboard(ImmutableViewportMetrics metrics, PointF velocity, DisplayPortMetrics displayPort) {
            // calculate the danger zone amounts based on the prefs
            float dangerZoneX = metrics.getWidth() * (DANGER_ZONE_BASE_X_MULTIPLIER + (velocity.x * DANGER_ZONE_INCR_X_MULTIPLIER));
            float dangerZoneY = metrics.getHeight() * (DANGER_ZONE_BASE_Y_MULTIPLIER + (velocity.y * DANGER_ZONE_INCR_Y_MULTIPLIER));
            // clamp it such that when added to the viewport, they don't exceed page size.
            // this is a prerequisite to calling shiftMarginsForPageBounds as we do below.
            dangerZoneX = Math.min(dangerZoneX, metrics.pageSizeWidth - metrics.getWidth());
            dangerZoneY = Math.min(dangerZoneY, metrics.pageSizeHeight - metrics.getHeight());

            // split the danger zone into margins based on velocity, and ensure it doesn't exceed
            // page bounds.
            RectF dangerMargins = velocityBiasedMargins(dangerZoneX, dangerZoneY, velocity);
            dangerMargins = shiftMarginsForPageBounds(dangerMargins, metrics);

            // we're about to checkerboard if the current viewport area + the danger zone margins
            // fall out of the current displayport anywhere.
            RectF adjustedViewport = new RectF(
                    metrics.viewportRectLeft - dangerMargins.left,
                    metrics.viewportRectTop - dangerMargins.top,
                    metrics.viewportRectRight + dangerMargins.right,
                    metrics.viewportRectBottom + dangerMargins.bottom);
            return !displayPort.contains(adjustedViewport);
        }

        @Override
        public String toString() {
            return "VelocityBiasStrategy mult=" + SIZE_MULTIPLIER + ", threshold=" + VELOCITY_THRESHOLD + ", reverse=" + REVERSE_BUFFER
                + ", dangerBaseX=" + DANGER_ZONE_BASE_X_MULTIPLIER + ", dangerBaseY=" + DANGER_ZONE_BASE_Y_MULTIPLIER
                + ", dangerIncrX=" + DANGER_ZONE_INCR_Y_MULTIPLIER + ", dangerIncrY=" + DANGER_ZONE_INCR_Y_MULTIPLIER;
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
    private static class DynamicResolutionStrategy extends DisplayPortStrategy {
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

        DynamicResolutionStrategy(Map<String, Integer> prefs) {
            // ignore prefs for now
        }

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

        @Override
        public String toString() {
            return "DynamicResolutionStrategy";
        }
    }

    /**
     * This class implements the variation where we use the draw time to predict where we will be when
     * a draw completes, and draw that instead of where we are now. In this variation, when our panning
     * speed drops below a certain threshold, we draw 9 viewports' worth of content so that the user can
     * pan in any direction without encountering checkerboarding.
     * Once the user is panning, we modify the displayport to encompass an area range of where we think
     * the user will be when the draw completes. This heuristic relies on both the estimated draw time
     * the panning velocity; unexpected changes in either of these values will cause the heuristic to
     * fail and show checkerboard.
     */
    private static class PredictionBiasStrategy extends DisplayPortStrategy {
        private static float VELOCITY_THRESHOLD;

        private int mPixelArea;         // area of the viewport, used in draw time calculations
        private int mMinFramesToDraw;   // minimum number of frames we take to draw
        private int mMaxFramesToDraw;   // maximum number of frames we take to draw

        PredictionBiasStrategy(Map<String, Integer> prefs) {
            VELOCITY_THRESHOLD = GeckoAppShell.getDpi() * getFloatPref(prefs, PREF_DISPLAYPORT_PB_VELOCITY_THRESHOLD, 16);
            resetPageState();
        }

        public DisplayPortMetrics calculate(ImmutableViewportMetrics metrics, PointF velocity) {
            float width = metrics.getWidth();
            float height = metrics.getHeight();
            mPixelArea = (int)(width * height);

            if (velocity.length() < VELOCITY_THRESHOLD) {
                // if we're going slow, expand the displayport to 9x viewport size
                RectF margins = new RectF(width, height, width, height);
                return getTileAlignedDisplayPortMetrics(margins, metrics.zoomFactor, metrics);
            }

            // figure out how far we expect to be
            float minDx = velocity.x * mMinFramesToDraw;
            float minDy = velocity.y * mMinFramesToDraw;
            float maxDx = velocity.x * mMaxFramesToDraw;
            float maxDy = velocity.y * mMaxFramesToDraw;

            // figure out how many pixels we will be drawing when we draw the above-calculated range.
            // this will be larger than the viewport area.
            float pixelsToDraw = (width + Math.abs(maxDx - minDx)) * (height + Math.abs(maxDy - minDy));
            // adjust how far we will get because of the time spent drawing all these extra pixels. this
            // will again increase the number of pixels drawn so really we could keep iterating this over
            // and over, but once seems enough for now.
            maxDx = maxDx * pixelsToDraw / mPixelArea;
            maxDy = maxDy * pixelsToDraw / mPixelArea;

            // and finally generate the displayport. the min/max stuff takes care of
            // negative velocities as well as positive.
            RectF margins = new RectF(
                -Math.min(minDx, maxDx),
                -Math.min(minDy, maxDy),
                Math.max(minDx, maxDx),
                Math.max(minDy, maxDy));
            return getTileAlignedDisplayPortMetrics(margins, metrics.zoomFactor, metrics);
        }

        public boolean aboutToCheckerboard(ImmutableViewportMetrics metrics, PointF velocity, DisplayPortMetrics displayPort) {
            // the code below is the same as in calculate() but is awkward to refactor since it has multiple outputs.
            // refer to the comments in calculate() to understand what this is doing.
            float minDx = velocity.x * mMinFramesToDraw;
            float minDy = velocity.y * mMinFramesToDraw;
            float maxDx = velocity.x * mMaxFramesToDraw;
            float maxDy = velocity.y * mMaxFramesToDraw;
            float pixelsToDraw = (metrics.getWidth() + Math.abs(maxDx - minDx)) * (metrics.getHeight() + Math.abs(maxDy - minDy));
            maxDx = maxDx * pixelsToDraw / mPixelArea;
            maxDy = maxDy * pixelsToDraw / mPixelArea;

            // now that we have an idea of how far we will be when the draw completes, take the farthest
            // end of that range and see if it falls outside the displayport bounds. if it does, allow
            // the draw to go through
            RectF predictedViewport = metrics.getViewport();
            predictedViewport.left += maxDx;
            predictedViewport.top += maxDy;
            predictedViewport.right += maxDx;
            predictedViewport.bottom += maxDy;

            predictedViewport = clampToPageBounds(predictedViewport, metrics);
            return !displayPort.contains(predictedViewport);
        }

        @Override
        public boolean drawTimeUpdate(long millis, int pixels) {
            // calculate the number of frames it took to draw a viewport-sized area
            float normalizedTime = (float)mPixelArea * (float)millis / (float)pixels;
            int normalizedFrames = (int)FloatMath.ceil(normalizedTime * 60f / 1000f);
            // broaden our range on how long it takes to draw if the draw falls outside
            // the range. this allows it to grow gradually. this heuristic may need to
            // be tweaked into more of a floating window average or something.
            if (normalizedFrames <= mMinFramesToDraw) {
                mMinFramesToDraw--;
            } else if (normalizedFrames > mMaxFramesToDraw) {
                mMaxFramesToDraw++;
            } else {
                return true;
            }
            Log.d(LOGTAG, "Widened draw range to [" + mMinFramesToDraw + ", " + mMaxFramesToDraw + "]");
            return true;
        }

        @Override
        public void resetPageState() {
            mMinFramesToDraw = 0;
            mMaxFramesToDraw = 2;
        }

        @Override
        public String toString() {
            return "PredictionBiasStrategy threshold=" + VELOCITY_THRESHOLD;
        }
    }
}
