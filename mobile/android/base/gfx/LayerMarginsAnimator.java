/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.util.FloatUtils;

import android.graphics.PointF;
import android.graphics.RectF;
import android.os.SystemClock;
import android.util.Log;
import android.view.animation.DecelerateInterpolator;

import java.util.Timer;
import java.util.TimerTask;

public class LayerMarginsAnimator {
    private static final String LOGTAG = "GeckoLayerMarginsAnimator";
    private static final float MS_PER_FRAME = 1000.0f / 60.0f;
    private static final long MARGIN_ANIMATION_DURATION = 250;

    /* This rect stores the maximum value margins can grow to when scrolling */
    private final RectF mMaxMargins;
    /* If this boolean is true, scroll changes will not affect margins */
    private boolean mMarginsPinned;
    /* The timer that handles showing/hiding margins */
    private Timer mAnimationTimer;
    /* This interpolator is used for the above mentioned animation */
    private final DecelerateInterpolator mInterpolator;
    /* The GeckoLayerClient whose margins will be animated */
    private final GeckoLayerClient mTarget;

    public LayerMarginsAnimator(GeckoLayerClient aTarget) {
        // Assign member variables from parameters
        mTarget = aTarget;

        // Create other member variables
        mMaxMargins = new RectF();
        mInterpolator = new DecelerateInterpolator();
    }

    /**
     * Sets the maximum values for margins to grow to, in pixels.
     */
    public synchronized void setMaxMargins(float left, float top, float right, float bottom) {
        mMaxMargins.set(left, top, right, bottom);

        // Update the Gecko-side global for fixed viewport margins.
        GeckoAppShell.sendEventToGecko(
            GeckoEvent.createBroadcastEvent("Viewport:FixedMarginsChanged",
                "{ \"top\" : " + top + ", \"right\" : " + right
                + ", \"bottom\" : " + bottom + ", \"left\" : " + left + " }"));
    }

    private void animateMargins(final float left, final float top, final float right, final float bottom, boolean immediately) {
        if (mAnimationTimer != null) {
            mAnimationTimer.cancel();
            mAnimationTimer = null;
        }

        if (immediately) {
            ImmutableViewportMetrics newMetrics = mTarget.getViewportMetrics().setMargins(left, top, right, bottom);
            mTarget.forceViewportMetrics(newMetrics, true, true);
            return;
        }

        ImmutableViewportMetrics metrics = mTarget.getViewportMetrics();

        final long startTime = SystemClock.uptimeMillis();
        final float startLeft = metrics.marginLeft;
        final float startTop = metrics.marginTop;
        final float startRight = metrics.marginRight;
        final float startBottom = metrics.marginBottom;

        mAnimationTimer = new Timer("Margin Animation Timer");
        mAnimationTimer.scheduleAtFixedRate(new TimerTask() {
            @Override
            public void run() {
                float progress = mInterpolator.getInterpolation(
                    Math.min(1.0f, (SystemClock.uptimeMillis() - startTime)
                                     / (float)MARGIN_ANIMATION_DURATION));

                synchronized(mTarget.getLock()) {
                    ImmutableViewportMetrics oldMetrics = mTarget.getViewportMetrics();
                    ImmutableViewportMetrics newMetrics = oldMetrics.setMargins(
                        FloatUtils.interpolate(startLeft, left, progress),
                        FloatUtils.interpolate(startTop, top, progress),
                        FloatUtils.interpolate(startRight, right, progress),
                        FloatUtils.interpolate(startBottom, bottom, progress));
                    PointF oldOffset = oldMetrics.getMarginOffset();
                    PointF newOffset = newMetrics.getMarginOffset();
                    newMetrics =
                        newMetrics.offsetViewportByAndClamp(newOffset.x - oldOffset.x,
                                                            newOffset.y - oldOffset.y);

                    if (progress >= 1.0f) {
                        mAnimationTimer.cancel();
                        mAnimationTimer = null;

                        // Force a redraw and update Gecko
                        mTarget.forceViewportMetrics(newMetrics, true, true);
                    } else {
                        mTarget.forceViewportMetrics(newMetrics, false, false);
                    }
                }
            }
        }, 0, (int)MS_PER_FRAME);
    }

    /**
     * Exposes the margin area by growing the margin components of the current
     * metrics to the values set in setMaxMargins.
     */
    public synchronized void showMargins(boolean immediately) {
        animateMargins(mMaxMargins.left, mMaxMargins.top, mMaxMargins.right, mMaxMargins.bottom, immediately);
    }

    public synchronized void hideMargins(boolean immediately) {
        animateMargins(0, 0, 0, 0, immediately);
    }

    public void setMarginsPinned(boolean pin) {
        mMarginsPinned = pin;
    }

    /*
     * Taking maximum margins into account, offsets the margins and then the
     * viewport origin and returns the modified metrics.
     */
    ImmutableViewportMetrics scrollBy(ImmutableViewportMetrics aMetrics, float aDx, float aDy) {
        // Make sure to cancel any margin animations when scrolling begins
        if (mAnimationTimer != null) {
            mAnimationTimer.cancel();
            mAnimationTimer = null;
        }

        float newMarginLeft = aMetrics.marginLeft;
        float newMarginTop = aMetrics.marginTop;
        float newMarginRight = aMetrics.marginRight;
        float newMarginBottom = aMetrics.marginBottom;

        // Only alter margins if the toolbar isn't pinned
        if (!mMarginsPinned) {
            RectF overscroll = aMetrics.getOverscroll();
            if (aDx >= 0) {
              // Scrolling right.
              float marginDx = Math.max(0, aDx - overscroll.left);
              newMarginLeft = aMetrics.marginLeft - Math.min(marginDx, aMetrics.marginLeft);
              newMarginRight = aMetrics.marginRight + Math.min(marginDx, mMaxMargins.right - aMetrics.marginRight);

              aDx -= aMetrics.marginLeft - newMarginLeft;
            } else {
              // Scrolling left.
              float marginDx = Math.max(0, -aDx - overscroll.right);
              newMarginLeft = aMetrics.marginLeft + Math.min(marginDx, mMaxMargins.left - aMetrics.marginLeft);
              newMarginRight = aMetrics.marginRight - Math.min(marginDx, aMetrics.marginRight);

              aDx -= aMetrics.marginLeft - newMarginLeft;
            }

            if (aDy >= 0) {
              // Scrolling down.
              float marginDy = Math.max(0, aDy - overscroll.top);
              newMarginTop = aMetrics.marginTop - Math.min(marginDy, aMetrics.marginTop);
              newMarginBottom = aMetrics.marginBottom + Math.min(marginDy, mMaxMargins.bottom - aMetrics.marginBottom);

              aDy -= aMetrics.marginTop - newMarginTop;
            } else {
              // Scrolling up.
              float marginDy = Math.max(0, -aDy - overscroll.bottom);
              newMarginTop = aMetrics.marginTop + Math.min(marginDy, mMaxMargins.top - aMetrics.marginTop);
              newMarginBottom = aMetrics.marginBottom - Math.min(marginDy, aMetrics.marginBottom);

              aDy -= aMetrics.marginTop - newMarginTop;
            }
        }

        return aMetrics.setMargins(newMarginLeft, newMarginTop, newMarginRight, newMarginBottom).offsetViewportBy(aDx, aDy);
    }
}
