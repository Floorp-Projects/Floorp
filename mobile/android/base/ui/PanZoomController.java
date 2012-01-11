/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009-2012
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Patrick Walton <pcwalton@mozilla.com>
 *   Kartikaya Gupta <kgupta@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko.ui;

import org.json.JSONObject;
import org.json.JSONException;
import org.mozilla.gecko.gfx.FloatSize;
import org.mozilla.gecko.gfx.LayerController;
import org.mozilla.gecko.gfx.PointUtils;
import org.mozilla.gecko.gfx.ViewportMetrics;
import org.mozilla.gecko.FloatUtils;
import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.GeckoEventListener;
import android.graphics.PointF;
import android.graphics.RectF;
import android.util.FloatMath;
import android.util.Log;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;
import java.util.Timer;
import java.util.TimerTask;

/*
 * Handles the kinetic scrolling and zooming physics for a layer controller.
 *
 * Many ideas are from Joe Hewitt's Scrollability:
 *   https://github.com/joehewitt/scrollability/
 */
public class PanZoomController
    extends GestureDetector.SimpleOnGestureListener
    implements ScaleGestureDetector.OnScaleGestureListener, GeckoEventListener
{
    private static final String LOGTAG = "GeckoPanZoomController";

    private static String MESSAGE_ZOOM_RECT = "Browser:ZoomToRect";
    private static String MESSAGE_ZOOM_PAGE = "Browser:ZoomToPageWidth";

    // Animation stops if the velocity is below this value when overscrolled or panning.
    private static final float STOPPED_THRESHOLD = 4.0f;
    // Animation stops is the velocity is below this threshold when flinging.
    private static final float FLING_STOPPED_THRESHOLD = 0.1f;
    // The distance the user has to pan before we recognize it as such (e.g. to avoid
    // 1-pixel pans between the touch-down and touch-up of a click). In units of inches.
    private static final float PAN_THRESHOLD = 0.1f;
    // Angle from axis within which we stay axis-locked
    private static final double AXIS_LOCK_ANGLE = Math.PI / 6.0; // 30 degrees
    // The maximum amount we allow you to zoom into a page
    private static final float MAX_ZOOM = 4.0f;

    /* 16 precomputed frames of the _ease-out_ animation from the CSS Transitions specification. */
    private static final float[] EASE_OUT_ANIMATION_FRAMES = {
        0.00000f,   /* 0 */
        0.10211f,   /* 1 */
        0.19864f,   /* 2 */
        0.29043f,   /* 3 */
        0.37816f,   /* 4 */
        0.46155f,   /* 5 */
        0.54054f,   /* 6 */
        0.61496f,   /* 7 */
        0.68467f,   /* 8 */
        0.74910f,   /* 9 */
        0.80794f,   /* 10 */
        0.86069f,   /* 11 */
        0.90651f,   /* 12 */
        0.94471f,   /* 13 */
        0.97401f,   /* 14 */
        0.99309f,   /* 15 */
    };

    private enum PanZoomState {
        NOTHING,        /* no touch-start events received */
        FLING,          /* all touches removed, but we're still scrolling page */
        TOUCHING,       /* one touch-start event received */
        PANNING_LOCKED, /* touch-start followed by move (i.e. panning with axis lock) */
        PANNING,        /* panning without axis lock */
        PANNING_HOLD,   /* in panning, but not moving.
                         * similar to TOUCHING but after starting a pan */
        PANNING_HOLD_LOCKED, /* like PANNING_HOLD, but axis lock still in effect */
        PINCHING,       /* nth touch-start, where n > 1. this mode allows pan and zoom */
        ANIMATED_ZOOM   /* animated zoom to a new rect */
    }

    private final LayerController mController;
    private final SubdocumentScrollHelper mSubscroller;
    private final Axis mX;
    private final Axis mY;

    /* The timer that handles flings or bounces. */
    private Timer mAnimationTimer;
    /* The runnable being scheduled by the animation timer. */
    private AnimationRunnable mAnimationRunnable;
    /* The zoom focus at the first zoom event (in page coordinates). */
    private PointF mLastZoomFocus;
    /* The time the last motion event took place. */
    private long mLastEventTime;
    /* Current state the pan/zoom UI is in. */
    private PanZoomState mState;

    public PanZoomController(LayerController controller) {
        mController = controller;
        mSubscroller = new SubdocumentScrollHelper(this);
        mX = new AxisX(mSubscroller);
        mY = new AxisY(mSubscroller);

        mState = PanZoomState.NOTHING;

        GeckoAppShell.registerGeckoEventListener(MESSAGE_ZOOM_RECT, this);
        GeckoAppShell.registerGeckoEventListener(MESSAGE_ZOOM_PAGE, this);
    }

    public void handleMessage(String event, JSONObject message) {
        Log.i(LOGTAG, "Got message: " + event);
        try {
            if (MESSAGE_ZOOM_RECT.equals(event)) {
                float scale = mController.getZoomFactor();
                float x = (float)message.getDouble("x");
                float y = (float)message.getDouble("y");
                final RectF zoomRect = new RectF(x, y,
                                     x + (float)message.getDouble("w"),
                                     y + (float)message.getDouble("h"));
                mController.post(new Runnable() {
                    public void run() {
                        animatedZoomTo(zoomRect);
                    }
                });
            } else if (MESSAGE_ZOOM_PAGE.equals(event)) {
                float scale = mController.getZoomFactor();
                FloatSize pageSize = mController.getPageSize();

                RectF viewableRect = mController.getViewport();
                float y = viewableRect.top;
                // attempt to keep zoom keep focused on the center of the viewport
                float dh = viewableRect.height()*(1 - pageSize.width/viewableRect.width()); // increase in the height
                final RectF r = new RectF(0.0f,
                                    y + dh/2,
                                    pageSize.width,
                                    (y + pageSize.width * viewableRect.height()/viewableRect.width()));
                mController.post(new Runnable() {
                    public void run() {
                        animatedZoomTo(r);
                    }
                });
            }
        } catch (Exception e) {
            Log.e(LOGTAG, "Exception handling message \"" + event + "\":", e);
        }
    }

    public boolean onTouchEvent(MotionEvent event) {
        switch (event.getAction() & event.ACTION_MASK) {
        case MotionEvent.ACTION_DOWN:   return onTouchStart(event);
        case MotionEvent.ACTION_MOVE:   return onTouchMove(event);
        case MotionEvent.ACTION_UP:     return onTouchEnd(event);
        case MotionEvent.ACTION_CANCEL: return onTouchCancel(event);
        default:                        return false;
        }
    }

    /** This function must be called from the UI thread. */
    @SuppressWarnings("fallthrough")
    public void abortAnimation() {
        // this happens when gecko changes the viewport on us or if the device is rotated.
        // if that's the case, abort any animation in progress and re-zoom so that the page
        // snaps to edges. for other cases (where the user's finger(s) are down) don't do
        // anything special.
        switch (mState) {
        case FLING:
            mX.stopFling();
            mY.stopFling();
            mState = PanZoomState.NOTHING;
            // fall through
        case ANIMATED_ZOOM:
            // the zoom that's in progress likely makes no sense any more (such as if
            // the screen orientation changed) so abort it
            // fall through
        case NOTHING:
            // Don't do animations here; they're distracting and can cause flashes on page
            // transitions.
            mController.setViewportMetrics(getValidViewportMetrics());
            mController.notifyLayerClientOfGeometryChange();
            break;
        }
    }

    /*
     * Panning/scrolling
     */

    private boolean onTouchStart(MotionEvent event) {
        Log.d(LOGTAG, "onTouchStart in state " + mState);
        // user is taking control of movement, so stop
        // any auto-movement we have going
        stopAnimationTimer();
        mSubscroller.cancel();

        switch (mState) {
        case ANIMATED_ZOOM:
            return false;
        case FLING:
        case NOTHING:
            startTouch(event.getX(0), event.getY(0), event.getEventTime());
            return false;
        case TOUCHING:
        case PANNING:
        case PANNING_LOCKED:
        case PANNING_HOLD:
        case PANNING_HOLD_LOCKED:
        case PINCHING:
            Log.e(LOGTAG, "Received impossible touch down while in " + mState);
            return false;
        }
        Log.e(LOGTAG, "Unhandled case " + mState + " in onTouchStart");
        return false;
    }

    @SuppressWarnings("fallthrough")
    private boolean onTouchMove(MotionEvent event) {
        Log.d(LOGTAG, "onTouchMove in state " + mState);

        switch (mState) {
        case NOTHING:
        case FLING:
            // should never happen
            Log.e(LOGTAG, "Received impossible touch move while in " + mState);
            return false;
        case TOUCHING:
            if (panDistance(event) < PAN_THRESHOLD * GeckoAppShell.getDpi()) {
                return false;
            }
            cancelTouch();
            // fall through
        case PANNING_HOLD_LOCKED:
            GeckoApp.mAppContext.mAutoCompletePopup.hide();
            mState = PanZoomState.PANNING_LOCKED;
            // fall through
        case PANNING_LOCKED:
            track(event);
            return true;
        case PANNING_HOLD:
            GeckoApp.mAppContext.mAutoCompletePopup.hide();
            mState = PanZoomState.PANNING;
            // fall through
        case PANNING:
            track(event);
            return true;
        case ANIMATED_ZOOM:
        case PINCHING:
            // scale gesture listener will handle this
            return false;
        }
        Log.e(LOGTAG, "Unhandled case " + mState + " in onTouchMove");
        return false;
    }

    private boolean onTouchEnd(MotionEvent event) {
        Log.d(LOGTAG, "onTouchEnd in " + mState);

        switch (mState) {
        case NOTHING:
        case FLING:
            // should never happen
            Log.e(LOGTAG, "Received impossible touch end while in " + mState);
            return false;
        case TOUCHING:
            mState = PanZoomState.NOTHING;
            // the switch into TOUCHING might have happened while the page was
            // snapping back after overscroll. we need to finish the snap if that
            // was the case
            bounce();
            return false;
        case PANNING:
        case PANNING_LOCKED:
        case PANNING_HOLD:
        case PANNING_HOLD_LOCKED:
            mState = PanZoomState.FLING;
            fling();
            return true;
        case PINCHING:
            mState = PanZoomState.NOTHING;
            return true;
        case ANIMATED_ZOOM:
            return false;
        }
        Log.e(LOGTAG, "Unhandled case " + mState + " in onTouchEnd");
        return false;
    }

    private boolean onTouchCancel(MotionEvent event) {
        Log.d(LOGTAG, "onTouchCancel in " + mState);

        mState = PanZoomState.NOTHING;
        // ensure we snap back if we're overscrolled
        bounce();
        return false;
    }

    private void startTouch(float x, float y, long time) {
        mX.startTouch(x);
        mY.startTouch(y);
        mState = PanZoomState.TOUCHING;
        mLastEventTime = time;
    }

    private float panDistance(MotionEvent move) {
        float dx = mX.panDistance(move.getX(0));
        float dy = mY.panDistance(move.getY(0));
        return FloatMath.sqrt(dx * dx + dy * dy);
    }

    private void track(float x, float y, long time) {
        float timeDelta = (float)(time - mLastEventTime);
        if (FloatUtils.fuzzyEquals(timeDelta, 0)) {
            // probably a duplicate event, ignore it. using a zero timeDelta will mess
            // up our velocity
            return;
        }
        mLastEventTime = time;

        if (mState == PanZoomState.PANNING_LOCKED) {
            // check to see if we should break the axis lock
            double angle = Math.atan2(mY.panDistance(y), mX.panDistance(x)); // range [-pi, pi]
            angle = Math.abs(angle); // range [0, pi]
            if (angle < AXIS_LOCK_ANGLE || angle > (Math.PI - AXIS_LOCK_ANGLE)) {
                // lock to x-axis
                mX.setLocked(false);
                mY.setLocked(true);
            } else if (Math.abs(angle - (Math.PI / 2)) < AXIS_LOCK_ANGLE) {
                // lock to y-axis
                mX.setLocked(true);
                mY.setLocked(false);
            } else {
                // break axis lock but log the angle so we can fine-tune this when people complain
                mState = PanZoomState.PANNING;
                mX.setLocked(false);
                mY.setLocked(false);
                angle = Math.abs(angle - (Math.PI / 2));  // range [0, pi/2]
            }
        }

        mX.updateWithTouchAt(x, timeDelta);
        mY.updateWithTouchAt(y, timeDelta);
    }

    private void track(MotionEvent event) {
        mX.saveTouchPos();
        mY.saveTouchPos();

        for (int i = 0; i < event.getHistorySize(); i++) {
            track(event.getHistoricalX(0, i),
                  event.getHistoricalY(0, i),
                  event.getHistoricalEventTime(i));
        }
        track(event.getX(0), event.getY(0), event.getEventTime());

        if (stopped()) {
            if (mState == PanZoomState.PANNING) {
                mState = PanZoomState.PANNING_HOLD;
            } else if (mState == PanZoomState.PANNING_LOCKED) {
                mState = PanZoomState.PANNING_HOLD_LOCKED;
            } else {
                // should never happen, but handle anyway for robustness
                Log.e(LOGTAG, "Impossible case " + mState + " when stopped in track");
                mState = PanZoomState.PANNING_HOLD_LOCKED;
            }
        }

        mX.startPan();
        mY.startPan();
        updatePosition();
    }

    private void fling() {
        updatePosition();

        stopAnimationTimer();

        boolean stopped = stopped();
        mX.startFling(stopped);
        mY.startFling(stopped);

        startAnimationTimer(new FlingRunnable());
    }

    /* Performs a bounce-back animation to the given viewport metrics. */
    private void bounce(ViewportMetrics metrics) {
        stopAnimationTimer();

        ViewportMetrics bounceStartMetrics = new ViewportMetrics(mController.getViewportMetrics());
        if (bounceStartMetrics.fuzzyEquals(metrics)) {
            mState = PanZoomState.NOTHING;
            return;
        }

        mState = PanZoomState.FLING;
        Log.d(LOGTAG, "end bounce at " + metrics);

        startAnimationTimer(new BounceRunnable(bounceStartMetrics, metrics));
    }

    /* Performs a bounce-back animation to the nearest valid viewport metrics. */
    private void bounce() {
        bounce(getValidViewportMetrics());
    }

    /* Starts the fling or bounce animation. */
    private void startAnimationTimer(final AnimationRunnable runnable) {
        if (mAnimationTimer != null) {
            Log.e(LOGTAG, "Attempted to start a new fling without canceling the old one!");
            stopAnimationTimer();
        }

        mAnimationTimer = new Timer("Animation Timer");
        mAnimationRunnable = runnable;
        mAnimationTimer.scheduleAtFixedRate(new TimerTask() {
            @Override
            public void run() { mController.post(runnable); }
        }, 0, 1000L/60L);
    }

    /* Stops the fling or bounce animation. */
    private void stopAnimationTimer() {
        if (mAnimationTimer != null) {
            mAnimationTimer.cancel();
            mAnimationTimer = null;
        }
        if (mAnimationRunnable != null) {
            mAnimationRunnable.terminate();
            mAnimationRunnable = null;
        }
    }

    private float getVelocity() {
        float xvel = mX.getRealVelocity();
        float yvel = mY.getRealVelocity();
        return FloatMath.sqrt(xvel * xvel + yvel * yvel);
    }

    private boolean stopped() {
        return getVelocity() < STOPPED_THRESHOLD;
    }

    PointF getDisplacement() {
        return new PointF(mX.resetDisplacement(), mY.resetDisplacement());
    }

    private void updatePosition() {
        mX.displace();
        mY.displace();
        PointF displacement = getDisplacement();
        if (! mSubscroller.scrollBy(displacement)) {
            synchronized (mController) {
                mController.scrollBy(displacement);
            }
        }
    }

    private abstract class AnimationRunnable implements Runnable {
        private boolean mAnimationTerminated;

        /* This should always run on the UI thread */
        public final void run() {
            /*
             * Since the animation timer queues this runnable on the UI thread, it
             * is possible that even when the animation timer is cancelled, there
             * are multiple instances of this queued, so we need to have another
             * mechanism to abort. This is done by using the mAnimationTerminated flag.
             */
            if (mAnimationTerminated) {
                return;
            }
            animateFrame();
        }

        protected abstract void animateFrame();

        /* This should always run on the UI thread */
        protected final void terminate() {
            mAnimationTerminated = true;
        }
    }

    /* The callback that performs the bounce animation. */
    private class BounceRunnable extends AnimationRunnable {
        /* The current frame of the bounce-back animation */
        private int mBounceFrame;
        /*
         * The viewport metrics that represent the start and end of the bounce-back animation,
         * respectively.
         */
        private ViewportMetrics mBounceStartMetrics;
        private ViewportMetrics mBounceEndMetrics;

        BounceRunnable(ViewportMetrics startMetrics, ViewportMetrics endMetrics) {
            mBounceStartMetrics = startMetrics;
            mBounceEndMetrics = endMetrics;
        }

        protected void animateFrame() {
            /*
             * The pan/zoom controller might have signaled to us that it wants to abort the
             * animation by setting the state to PanZoomState.NOTHING. Handle this case and bail
             * out.
             */
            if (mState != PanZoomState.FLING) {
                finishAnimation();
                return;
            }

            /* Perform the next frame of the bounce-back animation. */
            if (mBounceFrame < EASE_OUT_ANIMATION_FRAMES.length) {
                advanceBounce();
                return;
            }

            /* Finally, if there's nothing else to do, complete the animation and go to sleep. */
            finishBounce();
            finishAnimation();
            mState = PanZoomState.NOTHING;
        }

        /* Performs one frame of a bounce animation. */
        private void advanceBounce() {
            synchronized (mController) {
                float t = EASE_OUT_ANIMATION_FRAMES[mBounceFrame];
                ViewportMetrics newMetrics = mBounceStartMetrics.interpolate(mBounceEndMetrics, t);
                mController.setViewportMetrics(newMetrics);
                mController.notifyLayerClientOfGeometryChange();
                mBounceFrame++;
            }
        }

        /* Concludes a bounce animation and snaps the viewport into place. */
        private void finishBounce() {
            synchronized (mController) {
                mController.setViewportMetrics(mBounceEndMetrics);
                mController.notifyLayerClientOfGeometryChange();
                mBounceFrame = -1;
            }
        }
    }

    // The callback that performs the fling animation.
    private class FlingRunnable extends AnimationRunnable {
        protected void animateFrame() {
            /*
             * The pan/zoom controller might have signaled to us that it wants to abort the
             * animation by setting the state to PanZoomState.NOTHING. Handle this case and bail
             * out.
             */
            if (mState != PanZoomState.FLING) {
                finishAnimation();
                return;
            }

            /* Advance flings, if necessary. */
            boolean flingingX = mX.advanceFling();
            boolean flingingY = mY.advanceFling();

            boolean overscrolled = ((mX.overscrolled() || mY.overscrolled()) && !mSubscroller.scrolling());

            /* If we're still flinging in any direction, update the origin. */
            if (flingingX || flingingY) {
                updatePosition();

                /*
                 * Check to see if we're still flinging with an appreciable velocity. The threshold is
                 * higher in the case of overscroll, so we bounce back eagerly when overscrolling but
                 * coast smoothly to a stop when not. In other words, require a greater velocity to
                 * maintain the fling once we enter overscroll.
                 */
                float threshold = (overscrolled ? STOPPED_THRESHOLD : FLING_STOPPED_THRESHOLD);
                if (getVelocity() >= threshold) {
                    // we're still flinging
                    return;
                }

                mX.stopFling();
                mY.stopFling();
            }

            /*
             * Perform a bounce-back animation if overscrolled, unless panning is being
             * handled by the subwindow scroller.
             */
            if (overscrolled) {
                bounce();
            } else {
                finishAnimation();
                mState = PanZoomState.NOTHING;
            }
        }
    }

    private void finishAnimation() {
        Log.d(LOGTAG, "Finishing animation at " + mController.getViewportMetrics());
        stopAnimationTimer();

        // Force a viewport synchronisation
        mController.setForceRedraw();
        mController.notifyLayerClientOfGeometryChange();
    }

    /* Returns the nearest viewport metrics with no overscroll visible. */
    private ViewportMetrics getValidViewportMetrics() {
        ViewportMetrics viewportMetrics = new ViewportMetrics(mController.getViewportMetrics());
        Log.d(LOGTAG, "generating valid viewport using " + viewportMetrics);

        /* First, we adjust the zoom factor so that we can make no overscrolled area visible. */
        float zoomFactor = viewportMetrics.getZoomFactor();
        FloatSize pageSize = viewportMetrics.getPageSize();
        RectF viewport = viewportMetrics.getViewport();

        float minZoomFactor = 0.0f;
        if (viewport.width() > pageSize.width && pageSize.width > 0) {
            float scaleFactor = viewport.width() / pageSize.width;
            minZoomFactor = Math.max(minZoomFactor, zoomFactor * scaleFactor);
        }
        if (viewport.height() > pageSize.height && pageSize.height > 0) {
            float scaleFactor = viewport.height() / pageSize.height;
            minZoomFactor = Math.max(minZoomFactor, zoomFactor * scaleFactor);
        }

        if (!FloatUtils.fuzzyEquals(minZoomFactor, 0.0f)) {
            PointF center = new PointF(viewport.width() / 2.0f, viewport.height() / 2.0f);
            viewportMetrics.scaleTo(minZoomFactor, center);
        } else if (zoomFactor > MAX_ZOOM) {
            PointF center = new PointF(viewport.width() / 2.0f, viewport.height() / 2.0f);
            viewportMetrics.scaleTo(MAX_ZOOM, center);
        }

        /* Now we pan to the right origin. */
        viewportMetrics.setViewport(viewportMetrics.getClampedViewport());
        Log.d(LOGTAG, "generated valid viewport as " + viewportMetrics);

        return viewportMetrics;
    }

    private class AxisX extends Axis {
        AxisX(SubdocumentScrollHelper subscroller) { super(subscroller); }
        @Override
        public float getOrigin() { return mController.getOrigin().x; }
        @Override
        protected float getViewportLength() { return mController.getViewportSize().width; }
        @Override
        protected float getPageLength() { return mController.getPageSize().width; }
    }

    private class AxisY extends Axis {
        AxisY(SubdocumentScrollHelper subscroller) { super(subscroller); }
        @Override
        public float getOrigin() { return mController.getOrigin().y; }
        @Override
        protected float getViewportLength() { return mController.getViewportSize().height; }
        @Override
        protected float getPageLength() { return mController.getPageSize().height; }
    }

    /*
     * Zooming
     */
    @Override
    public boolean onScaleBegin(ScaleGestureDetector detector) {
        Log.d(LOGTAG, "onScaleBegin in " + mState);

        if (mState == PanZoomState.ANIMATED_ZOOM)
            return false;

        mState = PanZoomState.PINCHING;
        mLastZoomFocus = new PointF(detector.getFocusX(), detector.getFocusY());
        GeckoApp.mAppContext.hidePluginViews();
        GeckoApp.mAppContext.mAutoCompletePopup.hide();
        cancelTouch();

        return true;
    }

    @Override
    public boolean onScale(ScaleGestureDetector detector) {
        Log.d(LOGTAG, "onScale in state " + mState);

        if (mState == PanZoomState.ANIMATED_ZOOM)
            return false;

        float prevSpan = detector.getPreviousSpan();
        if (FloatUtils.fuzzyEquals(prevSpan, 0.0f)) {
            // let's eat this one to avoid setting the new zoom to infinity (bug 711453)
            return true;
        }

        float spanRatio = detector.getCurrentSpan() / prevSpan;

        /*
         * Apply edge resistance if we're zoomed out smaller than the page size by scaling the zoom
         * factor toward 1.0.
         */
        float resistance = Math.min(mX.getEdgeResistance(), mY.getEdgeResistance());
        if (spanRatio > 1.0f)
            spanRatio = 1.0f + (spanRatio - 1.0f) * resistance;
        else
            spanRatio = 1.0f - (1.0f - spanRatio) * resistance;

        synchronized (mController) {
            float newZoomFactor = mController.getZoomFactor() * spanRatio;
            if (newZoomFactor >= MAX_ZOOM) {
                // apply resistance when zooming past MAX_ZOOM,
                // such that it asymptotically reaches MAX_ZOOM + 1.0
                // but never exceeds that
                float excessZoom = newZoomFactor - MAX_ZOOM;
                excessZoom = 1.0f - (float)Math.exp(-excessZoom);
                newZoomFactor = MAX_ZOOM + excessZoom;
            }

            mController.scrollBy(new PointF(mLastZoomFocus.x - detector.getFocusX(),
                                            mLastZoomFocus.y - detector.getFocusY()));
            PointF focus = new PointF(detector.getFocusX(), detector.getFocusY());
            mController.scaleWithFocus(newZoomFactor, focus);
        }

        mLastZoomFocus.set(detector.getFocusX(), detector.getFocusY());

        return true;
    }

    @Override
    public void onScaleEnd(ScaleGestureDetector detector) {
        Log.d(LOGTAG, "onScaleEnd in " + mState);

        if (mState == PanZoomState.ANIMATED_ZOOM)
            return;

        // switch back to the touching state
        startTouch(detector.getFocusX(), detector.getFocusY(), detector.getEventTime());

        // Force a viewport synchronisation
        mController.setForceRedraw();
        mController.notifyLayerClientOfGeometryChange();
        GeckoApp.mAppContext.showPluginViews();
    }

    public boolean getRedrawHint() {
        return (mState == PanZoomState.NOTHING || mState == PanZoomState.FLING);
    }

    private void sendPointToGecko(String event, MotionEvent motionEvent) {
        String json;
        try {
            PointF point = new PointF(motionEvent.getX(), motionEvent.getY());
            point = mController.convertViewPointToLayerPoint(point);
            if (point == null) {
                return;
            }
            json = PointUtils.toJSON(point).toString();
        } catch (Exception e) {
            Log.e(LOGTAG, "Unable to convert point to JSON for " + event, e);
            return;
        }

        GeckoAppShell.sendEventToGecko(new GeckoEvent(event, json));
    }

    @Override
    public void onLongPress(MotionEvent motionEvent) {
        sendPointToGecko("Gesture:LongPress", motionEvent);
    }

    @Override
    public boolean onDown(MotionEvent motionEvent) {
        sendPointToGecko("Gesture:ShowPress", motionEvent);
        return false;
    }

    @Override
    public boolean onSingleTapConfirmed(MotionEvent motionEvent) {
        GeckoApp.mAppContext.mAutoCompletePopup.hide();
        sendPointToGecko("Gesture:SingleTap", motionEvent);
        return true;
    }

    @Override
    public boolean onDoubleTap(MotionEvent motionEvent) {
        sendPointToGecko("Gesture:DoubleTap", motionEvent);
        return true;
    }

    private void cancelTouch() {
        GeckoEvent e = new GeckoEvent("Gesture:CancelTouch", "");
        GeckoAppShell.sendEventToGecko(e);
    }

    private boolean animatedZoomTo(RectF zoomToRect) {
        GeckoApp.mAppContext.hidePluginViews();
        GeckoApp.mAppContext.mAutoCompletePopup.hide();

        mState = PanZoomState.ANIMATED_ZOOM;
        final float startZoom = mController.getZoomFactor();
        final PointF startPoint = mController.getOrigin();

        RectF viewport = mController.getViewport();

        float newHeight = zoomToRect.width() * viewport.height() / viewport.width();
        // if the requested rect would not fill the screen, shift it to be centered
        if (zoomToRect.height() < newHeight) {
            zoomToRect.top -= (newHeight - zoomToRect.height())/2;
            zoomToRect.bottom = zoomToRect.top + newHeight;
        }

        zoomToRect = mController.restrictToPageSize(zoomToRect);
        float finalZoom = viewport.width() * startZoom / zoomToRect.width();

        ViewportMetrics finalMetrics = new ViewportMetrics(mController.getViewportMetrics());
        finalMetrics.setOrigin(new PointF(zoomToRect.left, zoomToRect.top));
        finalMetrics.scaleTo(finalZoom, new PointF(0.0f, 0.0f));

        bounce(finalMetrics);
        return true;
    }
}
