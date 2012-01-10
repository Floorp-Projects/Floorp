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
 * Portions created by the Initial Developer are Copyright (C) 2009-2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Patrick Walton <pcwalton@mozilla.com>
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
import android.util.Log;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;
import java.lang.Math;
import java.util.Date;
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

    private LayerController mController;

    // This fraction of velocity remains after every animation frame when the velocity is low.
    private static final float FRICTION_SLOW = 0.85f;
    // This fraction of velocity remains after every animation frame when the velocity is high.
    private static final float FRICTION_FAST = 0.97f;
    // Below this velocity (in pixels per frame), the friction starts increasing from FRICTION_FAST
    // to FRICTION_SLOW.
    private static final float VELOCITY_THRESHOLD = 10.0f;
    // Animation stops if the velocity is below this value when overscrolled or panning.
    private static final float STOPPED_THRESHOLD = 4.0f;
    // Animation stops is the velocity is below this threshold when flinging.
    private static final float FLING_STOPPED_THRESHOLD = 0.1f;
    // The percentage of the surface which can be overscrolled before it must snap back.
    private static final float SNAP_LIMIT = 0.75f;
    // The rate of deceleration when the surface has overscrolled.
    private static final float OVERSCROLL_DECEL_RATE = 0.04f;
    // The distance the user has to pan before we recognize it as such (e.g. to avoid
    // 1-pixel pans between the touch-down and touch-up of a click). In units of inches.
    private static final float PAN_THRESHOLD = 0.1f;
    // Angle from axis within which we stay axis-locked
    private static final double AXIS_LOCK_ANGLE = Math.PI / 6.0; // 30 degrees
    // The maximum velocity change factor between events, per ms, in %.
    // Direction changes are excluded.
    private static final float MAX_EVENT_ACCELERATION = 0.012f;
    // The minimum amount of space that must be present for an axis to be considered scrollable,
    // in pixels.
    private static final float MIN_SCROLLABLE_DISTANCE = 0.5f;
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

    /* The timer that handles flings or bounces. */
    private Timer mAnimationTimer;
    /* The runnable being scheduled by the animation timer. */
    private AnimationRunnable mAnimationRunnable;
    /* Information about the X axis. */
    private AxisX mX;
    /* Information about the Y axis. */
    private AxisY mY;
    /* The zoom focus at the first zoom event (in page coordinates). */
    private PointF mLastZoomFocus;
    /* The time the last motion event took place. */
    private long mLastEventTime;

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

    private PanZoomState mState;

    private boolean mOverridePanning;
    private boolean mOverrideScrollAck;
    private boolean mOverrideScrollPending;

    public PanZoomController(LayerController controller) {
        mController = controller;
        mX = new AxisX(); mY = new AxisY();
        mState = PanZoomState.NOTHING;

        GeckoAppShell.registerGeckoEventListener("Browser:ZoomToRect", this);
        GeckoAppShell.registerGeckoEventListener("Browser:ZoomToPageWidth", this);
        GeckoAppShell.registerGeckoEventListener("Panning:Override", this);
        GeckoAppShell.registerGeckoEventListener("Panning:CancelOverride", this);
        GeckoAppShell.registerGeckoEventListener("Gesture:ScrollAck", this);
    }

    public void handleMessage(String event, JSONObject message) {
        Log.i(LOGTAG, "Got message: " + event);
        try {
            if ("Panning:Override".equals(event)) {
                mOverridePanning = true;
                mOverrideScrollAck = true;
            } else if ("Panning:CancelOverride".equals(event)) {
                mOverridePanning = false;
            } else if ("Gesture:ScrollAck".equals(event)) {
                mController.post(new Runnable() {
                    public void run() {
                        mOverrideScrollAck = true;
                        if (mOverridePanning && mOverrideScrollPending)
                            updatePosition();
                    }
                });
            } else if (event.equals("Browser:ZoomToRect")) {
                if (mController != null) {
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
                }
            } else if (event.equals("Browser:ZoomToPageWidth")) {
                if (mController != null) {
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
            mX.velocity = mY.velocity = 0.0f;
            mState = PanZoomState.NOTHING;
            // fall through
        case ANIMATED_ZOOM:
            // the zoom that's in progress likely makes no sense any more (such as if
            // the screen orientation changed) so abort it and start a new one to
            // ensure the viewport doesn't contain out-of-bounds areas
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
        mOverridePanning = false;

        switch (mState) {
        case ANIMATED_ZOOM:
            return false;
        case FLING:
        case NOTHING:
            mState = PanZoomState.TOUCHING;
            mX.velocity = mY.velocity = 0.0f;
            mX.locked = mY.locked = false;
            mX.lastTouchPos = mX.firstTouchPos = mX.touchPos = event.getX(0);
            mY.lastTouchPos = mY.firstTouchPos = mY.touchPos = event.getY(0);
            mLastEventTime = event.getEventTime();
            return false;
        case TOUCHING:
        case PANNING:
        case PANNING_LOCKED:
        case PANNING_HOLD:
        case PANNING_HOLD_LOCKED:
        case PINCHING:
            mState = PanZoomState.PINCHING;
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
            if (panDistance(event) < PAN_THRESHOLD * GeckoAppShell.getDpi())
                return false;
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
            int points = event.getPointerCount();
            if (points == 1) {
                // last touch up
                mState = PanZoomState.NOTHING;
            } else if (points == 2) {
                int pointRemovedIndex = event.getActionIndex();
                int pointRemainingIndex = 1 - pointRemovedIndex; // kind of a hack
                mState = PanZoomState.TOUCHING;
                mX.firstTouchPos = mX.touchPos = event.getX(pointRemainingIndex);
                mX.firstTouchPos = mY.touchPos = event.getY(pointRemainingIndex);
            } else {
                // still pinching, do nothing
            }
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

    private float panDistance(MotionEvent move) {
        float dx = mX.firstTouchPos - move.getX(0);
        float dy = mY.firstTouchPos - move.getY(0);
        return (float)Math.sqrt(dx * dx + dy * dy);
    }

    private float clampByFactor(float oldValue, float newValue, float factor) {
        float maxChange = Math.abs(oldValue * factor);
        return Math.min(oldValue + maxChange, Math.max(oldValue - maxChange, newValue));
    }

    private void track(float x, float y, float lastX, float lastY, float timeDelta) {
        if (FloatUtils.fuzzyEquals(timeDelta, 0)) {
            // probably a duplicate event, ignore it. using a zero timeDelta will mess
            // up our velocity
            return;
        }

        if (mState == PanZoomState.PANNING_LOCKED) {
            // check to see if we should break the axis lock
            double angle = Math.atan2(y - mY.firstTouchPos, x - mX.firstTouchPos); // range [-pi, pi]
            angle = Math.abs(angle); // range [0, pi]
            if (angle < AXIS_LOCK_ANGLE || angle > (Math.PI - AXIS_LOCK_ANGLE)) {
                // lock to x-axis
                mX.locked = false;
                mY.locked = true;
            } else if (Math.abs(angle - (Math.PI / 2)) < AXIS_LOCK_ANGLE) {
                // lock to y-axis
                mX.locked = true;
                mY.locked = false;
            } else {
                // break axis lock but log the angle so we can fine-tune this when people complain
                mState = PanZoomState.PANNING;
                mX.locked = mY.locked = false;
                angle = Math.abs(angle - (Math.PI / 2));  // range [0, pi/2]
                Log.i(LOGTAG, "Breaking axis lock at " + (angle * 180.0 / Math.PI) + " degrees");
            }
        }

        float newVelocityX = ((lastX - x) / timeDelta) * (1000.0f/60.0f);
        float newVelocityY = ((lastY - y) / timeDelta) * (1000.0f/60.0f);
        float maxChange = MAX_EVENT_ACCELERATION * timeDelta;

        // If there's a direction change, or current velocity is very low,
        // allow setting of the velocity outright. Otherwise, use the current
        // velocity and a maximum change factor to set the new velocity.
        if (Math.abs(mX.velocity) < 1.0f ||
            (((mX.velocity > 0) != (newVelocityX > 0)) &&
             !FloatUtils.fuzzyEquals(newVelocityX, 0.0f)))
            mX.velocity = newVelocityX;
        else
            mX.velocity = clampByFactor(mX.velocity, newVelocityX, maxChange);
        if (Math.abs(mY.velocity) < 1.0f ||
            (((mY.velocity > 0) != (newVelocityY > 0)) &&
             !FloatUtils.fuzzyEquals(newVelocityY, 0.0f)))
            mY.velocity = newVelocityY;
        else
            mY.velocity = clampByFactor(mY.velocity, newVelocityY, maxChange);
    }

    private void track(MotionEvent event) {
        mX.lastTouchPos = mX.touchPos;
        mY.lastTouchPos = mY.touchPos;

        for (int i = 0; i < event.getHistorySize(); i++) {
            float x = event.getHistoricalX(0, i);
            float y = event.getHistoricalY(0, i);
            long time = event.getHistoricalEventTime(i);

            float timeDelta = (float)(time - mLastEventTime);
            mLastEventTime = time;

            track(x, y, mX.touchPos, mY.touchPos, timeDelta);
            mX.touchPos = x; mY.touchPos = y;
        }

        float timeDelta = (float)(event.getEventTime() - mLastEventTime);
        mLastEventTime = event.getEventTime();

        track(event.getX(0), event.getY(0), mX.touchPos, mY.touchPos, timeDelta);

        mX.touchPos = event.getX(0);
        mY.touchPos = event.getY(0);

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

        mX.setFlingState(Axis.FlingStates.PANNING); mY.setFlingState(Axis.FlingStates.PANNING);
        mX.displace(mOverridePanning); mY.displace(mOverridePanning);
        updatePosition();
    }

    private void fling() {
        if (mState != PanZoomState.FLING)
            mX.velocity = mY.velocity = 0.0f;

        mX.disableSnap = mY.disableSnap = mOverridePanning;

        mX.displace(mOverridePanning); mY.displace(mOverridePanning);
        updatePosition();

        stopAnimationTimer();

        boolean stopped = stopped();
        mX.startFling(stopped); mY.startFling(stopped);

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
        mX.setFlingState(Axis.FlingStates.SNAPPING); mY.setFlingState(Axis.FlingStates.SNAPPING);
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

    private boolean stopped() {
        float absVelocity = (float)Math.sqrt(mX.velocity * mX.velocity +
                                             mY.velocity * mY.velocity);
        return absVelocity < STOPPED_THRESHOLD;
    }

    private void updatePosition() {
        if (mOverridePanning) {
            if (!mOverrideScrollAck) {
                mOverrideScrollPending = true;
                return;
            }

            mOverrideScrollPending = false;
            JSONObject json = new JSONObject();

            try {
                json.put("x", mX.displacement);
                json.put("y", mY.displacement);
            } catch (JSONException e) {
                Log.e(LOGTAG, "Error forming Gesture:Scroll message: " + e);
            }

            GeckoEvent e = new GeckoEvent("Gesture:Scroll", json.toString());
            GeckoAppShell.sendEventToGecko(e);
            mOverrideScrollAck = false;
        } else {
            synchronized (mController) {
                mController.scrollBy(new PointF(mX.displacement, mY.displacement));
            }
        }

        mX.displacement = mY.displacement = 0;
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
            boolean flingingX = mX.getFlingState() == Axis.FlingStates.FLINGING;
            boolean flingingY = mY.getFlingState() == Axis.FlingStates.FLINGING;
            if (flingingX)
                mX.advanceFling();
            if (flingingY)
                mY.advanceFling();

            /* If we're still flinging in any direction, update the origin. */
            if (flingingX || flingingY) {
                mX.displace(mOverridePanning); mY.displace(mOverridePanning);
                updatePosition();

                /*
                 * If we're still flinging with an appreciable velocity, stop here. The threshold is
                 * higher in the case of overscroll, so we bounce back eagerly when overscrolling but
                 * coast smoothly to a stop when not.
                 */
                float excess = PointUtils.distance(new PointF(mX.getExcess(), mY.getExcess()));
                PointF velocityVector = new PointF(mX.getRealVelocity(), mY.getRealVelocity());
                float threshold = (excess >= 1.0f) ? STOPPED_THRESHOLD : FLING_STOPPED_THRESHOLD;
                if (PointUtils.distance(velocityVector) >= threshold)
                    return;
            }

            /*
             * Perform a bounce-back animation if overscrolled, unless panning is being overridden
             * (which happens e.g. when the user is panning an iframe).
             */
            boolean overscrolledX = mX.getOverscroll() != Axis.Overscroll.NONE;
            boolean overscrolledY = mY.getOverscroll() != Axis.Overscroll.NONE;
            if (!mOverridePanning && (overscrolledX || overscrolledY)) {
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

    private float computeElasticity(float excess, float viewportLength) {
        return 1.0f - excess / (viewportLength * SNAP_LIMIT);
    }

    // Physics information for one axis (X or Y).
    private abstract static class Axis {
        public enum FlingStates {
            STOPPED,
            PANNING,
            FLINGING,
            WAITING_TO_SNAP,
            SNAPPING,
        }

        public enum Overscroll {
            NONE,
            MINUS,      // Overscrolled in the negative direction
            PLUS,       // Overscrolled in the positive direction
            BOTH,       // Overscrolled in both directions (page is zoomed to smaller than screen)
        }

        public float firstTouchPos;             /* Position of the first touch event on the current drag. */
        public float touchPos;                  /* Position of the most recent touch event on the current drag. */
        public float lastTouchPos;              /* Position of the touch event before touchPos. */
        public float velocity;                  /* Velocity in this direction. */
        public boolean locked;                  /* Whether movement on this axis is locked. */
        public boolean disableSnap;             /* Whether overscroll snapping is disabled. */

        private FlingStates mFlingState;        /* The fling state we're in on this axis. */

        public abstract float getOrigin();
        protected abstract float getViewportLength();
        protected abstract float getPageLength();

        public float displacement;

        private int mSnapFrame;
        private float mSnapPos, mSnapEndPos;

        public Axis() { mSnapFrame = -1; }

        public FlingStates getFlingState() { return mFlingState; }

        public void setFlingState(FlingStates aFlingState) {
            mFlingState = aFlingState;
        }

        private float getViewportEnd() { return getOrigin() + getViewportLength(); }

        public Overscroll getOverscroll() {
            boolean minus = (getOrigin() < 0.0f);
            boolean plus = (getViewportEnd() > getPageLength());
            if (minus && plus)
                return Overscroll.BOTH;
            else if (minus)
                return Overscroll.MINUS;
            else if (plus)
                return Overscroll.PLUS;
            else
                return Overscroll.NONE;
        }

        // Returns the amount that the page has been overscrolled. If the page hasn't been
        // overscrolled on this axis, returns 0.
        public float getExcess() {
            switch (getOverscroll()) {
            case MINUS:     return -getOrigin();
            case PLUS:      return getViewportEnd() - getPageLength();
            case BOTH:      return getViewportEnd() - getPageLength() - getOrigin();
            default:        return 0.0f;
            }
        }

        /*
         * Returns true if the page is zoomed in to some degree along this axis such that scrolling
         * is possible. Otherwise, returns false.
         */
        private boolean scrollable() {
            return getViewportLength() <= getPageLength() - MIN_SCROLLABLE_DISTANCE;
        }

        /*
         * Returns the resistance, as a multiplier, that should be taken into account when
         * tracking or pinching.
         */
        public float getEdgeResistance() {
            float excess = getExcess();
            return (excess > 0.0f) ? SNAP_LIMIT - excess / getViewportLength() : 1.0f;
        }

        /* Returns the velocity. If the axis is locked, returns 0. */
        public float getRealVelocity() {
            return locked ? 0.0f : velocity;
        }

        public void startFling(boolean stopped) {
            if (!stopped) {
                setFlingState(FlingStates.FLINGING);
                return;
            }

            if (disableSnap || FloatUtils.fuzzyEquals(getExcess(), 0.0f))
                setFlingState(FlingStates.STOPPED);
            else
                setFlingState(FlingStates.WAITING_TO_SNAP);
        }

        /* Advances a fling animation by one step. */
        public void advanceFling() {
            // If we aren't overscrolled, just apply friction.
            float excess = getExcess();
            if (disableSnap || FloatUtils.fuzzyEquals(excess, 0.0f)) {
                if (Math.abs(velocity) >= VELOCITY_THRESHOLD) {
                    velocity *= FRICTION_FAST;
                } else {
                    float t = velocity / VELOCITY_THRESHOLD;
                    velocity *= FloatUtils.interpolate(FRICTION_SLOW, FRICTION_FAST, t);
                }

                if (Math.abs(velocity) < FLING_STOPPED_THRESHOLD) {
                    velocity = 0.0f;
                    setFlingState(FlingStates.STOPPED);
                }
                return;
            }

            // Otherwise, decrease the velocity linearly.
            float elasticity = 1.0f - excess / (getViewportLength() * SNAP_LIMIT);
            if (getOverscroll() == Overscroll.MINUS)
                velocity = Math.min((velocity + OVERSCROLL_DECEL_RATE) * elasticity, 0.0f);
            else // must be Overscroll.PLUS
                velocity = Math.max((velocity - OVERSCROLL_DECEL_RATE) * elasticity, 0.0f);

            if (Math.abs(velocity) < 0.3f) {
                velocity = 0.0f;
                setFlingState(FlingStates.WAITING_TO_SNAP);
            }
        }

        // Performs displacement of the viewport position according to the current velocity.
        public void displace(boolean panningOverridden) {
            if (!panningOverridden && (locked || !scrollable()))
                return;

            if (mFlingState == FlingStates.PANNING)
                displacement += (lastTouchPos - touchPos) * getEdgeResistance();
            else
                displacement += velocity;
        }
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
        @Override
        public float getOrigin() { return mController.getOrigin().x; }
        @Override
        protected float getViewportLength() { return mController.getViewportSize().width; }
        @Override
        protected float getPageLength() { return mController.getPageSize().width; }
    }

    private class AxisY extends Axis {
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
    public void onScaleEnd(ScaleGestureDetector detector) {
        Log.d(LOGTAG, "onScaleEnd in " + mState);

        if (mState == PanZoomState.ANIMATED_ZOOM)
            return;

        mState = PanZoomState.PANNING_HOLD_LOCKED;
        mX.firstTouchPos = mX.lastTouchPos = mX.touchPos = detector.getFocusX();
        mY.firstTouchPos = mY.lastTouchPos = mY.touchPos = detector.getFocusY();

        // Force a viewport synchronisation
        mController.setForceRedraw();
        mController.notifyLayerClientOfGeometryChange();
        GeckoApp.mAppContext.showPluginViews();

        mState = PanZoomState.TOUCHING;
        mX.velocity = mY.velocity = 0.0f;
        mX.locked = mY.locked = false;
        mLastEventTime = detector.getEventTime();
    }

    @Override
    public void onLongPress(MotionEvent motionEvent) {
        String json;
        try {
            PointF point = new PointF(motionEvent.getX(), motionEvent.getY());
            point = mController.convertViewPointToLayerPoint(point);
            if (point == null) {
                return;
            }
            json = PointUtils.toJSON(point).toString();
        } catch(Exception ex) {
            Log.w(LOGTAG, "Error building return: " + ex);
            return; // json would be null
        }

        GeckoEvent e = new GeckoEvent("Gesture:LongPress", json);
        GeckoAppShell.sendEventToGecko(e);
    }

    public boolean getRedrawHint() {
        return (mState == PanZoomState.NOTHING || mState == PanZoomState.FLING);
    }

    @Override
    public boolean onDown(MotionEvent motionEvent) {
        String json;
        try {
            PointF point = new PointF(motionEvent.getX(), motionEvent.getY());
            point = mController.convertViewPointToLayerPoint(point);
            json = PointUtils.toJSON(point).toString();
        } catch(Exception ex) {
            throw new RuntimeException(ex);
        }

        GeckoEvent e = new GeckoEvent("Gesture:ShowPress", json);
        GeckoAppShell.sendEventToGecko(e);
        return false;
    }

    @Override
    public boolean onSingleTapConfirmed(MotionEvent motionEvent) {
        String json;
        try {
            PointF point = new PointF(motionEvent.getX(), motionEvent.getY());
            point = mController.convertViewPointToLayerPoint(point);
            json = PointUtils.toJSON(point).toString();
        } catch(Exception ex) {
            throw new RuntimeException(ex);
        }

        GeckoApp.mAppContext.mAutoCompletePopup.hide();

        GeckoEvent e = new GeckoEvent("Gesture:SingleTap", json);
        GeckoAppShell.sendEventToGecko(e);
        return true;
    }

    private void cancelTouch() {
        GeckoEvent e = new GeckoEvent("Gesture:CancelTouch", "");
        GeckoAppShell.sendEventToGecko(e);
    }

    @Override
    public boolean onDoubleTap(MotionEvent motionEvent) {
        String json;
        try {
            PointF point = new PointF(motionEvent.getX(), motionEvent.getY());
            point = mController.convertViewPointToLayerPoint(point);
            json = PointUtils.toJSON(point).toString();
        } catch(Exception ex) {
            throw new RuntimeException(ex);
        }

        GeckoEvent e = new GeckoEvent("Gesture:DoubleTap", json);
        GeckoAppShell.sendEventToGecko(e);
        return true;
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
