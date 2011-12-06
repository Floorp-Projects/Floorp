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
import org.mozilla.gecko.gfx.RectUtils;
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

    private static final float FRICTION = 0.85f;
    // Animation stops if the velocity is below this value.
    private static final float STOPPED_THRESHOLD = 4.0f;
    // The percentage of the surface which can be overscrolled before it must snap back.
    private static final float SNAP_LIMIT = 0.75f;
    // The rate of deceleration when the surface has overscrolled.
    private static final float OVERSCROLL_DECEL_RATE = 0.04f;
    // The duration of animation when bouncing back.
    private static final int SNAP_TIME = 240;
    // The number of subdivisions we should consider when plotting the ease-out transition. Higher
    // values make the animation more accurate, but slower to plot.
    private static final int SUBDIVISION_COUNT = 1000;
    // The distance the user has to pan before we recognize it as such (e.g. to avoid
    // 1-pixel pans between the touch-down and touch-up of a click). In units of inches.
    private static final float PAN_THRESHOLD = 0.1f;
    // Angle from axis within which we stay axis-locked
    private static final double AXIS_LOCK_ANGLE = Math.PI / 6.0; // 30 degrees
    // The maximum velocity change factor between events, per ms, in %.
    // Direction changes are excluded.
    private static final float MAX_EVENT_ACCELERATION = 0.012f;
    // Length of time to spend zooming. in ms
    public static final int ZOOM_DURATION         = 200;

    private Timer mFlingTimer;
    private Axis mX, mY;
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
        mX = new Axis(); mY = new Axis();
        mState = PanZoomState.NOTHING;

        populatePositionAndLength();

        GeckoAppShell.registerGeckoEventListener("Browser:ZoomToRect", this);
        GeckoAppShell.registerGeckoEventListener("Browser:ZoomToPageWidth", this);
        GeckoAppShell.registerGeckoEventListener("Panning:Override", this);
        GeckoAppShell.registerGeckoEventListener("Panning:CancelOverride", this);
        GeckoAppShell.registerGeckoEventListener("Gesture:ScrollAck", this);
    }

    protected void finalize() throws Throwable {
        GeckoAppShell.unregisterGeckoEventListener("Browser:ZoomToRect", this);
        GeckoAppShell.unregisterGeckoEventListener("Browser:ZoomToPageWidth", this);
        GeckoAppShell.unregisterGeckoEventListener("Panning:Override", this);
        GeckoAppShell.unregisterGeckoEventListener("Panning:CancelOverride", this);
        GeckoAppShell.unregisterGeckoEventListener("Gesture:ScrollAck", this);
        super.finalize();
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

    public void geometryChanged(boolean aAbortFling) {
        populatePositionAndLength();

        if (aAbortFling) {
            // this happens when gecko changes the viewport on us. if that's the case, abort
            // any fling that's in progress and re-fling so that the page snaps to edges. for
            // other cases (where the user's finger(s) are down) don't do anything special.
            switch (mState) {
            case FLING:
                mX.velocity = mY.velocity = 0.0f;
                mState = PanZoomState.NOTHING;
                // fall through
            case NOTHING:
                fling();
                break;
            }
        }
    }

    /*
     * Panning/scrolling
     */

    private boolean onTouchStart(MotionEvent event) {
        // user is taking control of movement, so stop
        // any auto-movement we have going
        if (mFlingTimer != null) {
            mFlingTimer.cancel();
            mFlingTimer = null;
        }
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

    private boolean onTouchMove(MotionEvent event) {
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
            mState = PanZoomState.PANNING_LOCKED;
            // fall through
        case PANNING_LOCKED:
            track(event);
            return true;
        case PANNING_HOLD:
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
            fling();
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
        mState = PanZoomState.NOTHING;
        // ensure we snap back if we're overscrolled
        fling();
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

        mX.setFlingState(Axis.FlingStates.PANNING);
        mY.setFlingState(Axis.FlingStates.PANNING);

        if (!mOverridePanning) {
            mX.applyEdgeResistance();
            mY.applyEdgeResistance();
        }
        mX.displace();
        mY.displace();

        updatePosition();
    }

    private void fling() {
        if (mState != PanZoomState.FLING)
            mX.velocity = mY.velocity = 0.0f;

        mX.disableSnap = mY.disableSnap = mOverridePanning;

        mX.displace(); mY.displace();
        updatePosition();

        if (mFlingTimer != null)
            mFlingTimer.cancel();

        boolean stopped = stopped();
        mX.startFling(stopped); mY.startFling(stopped);

        mFlingTimer = new Timer();
        mFlingTimer.scheduleAtFixedRate(new TimerTask() {
            public void run() { mController.post(new FlingRunnable()); }
        }, 0, 1000L/60L);
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
            mController.scrollBy(new PointF(mX.displacement, mY.displacement));
            mX.viewportPos += mX.displacement;
            mY.viewportPos += mY.displacement;
        }

        mX.displacement = mY.displacement = 0;
    }

    // Populates the viewport info and length in the axes.
    private void populatePositionAndLength() {
        FloatSize pageSize = mController.getPageSize();
        RectF visibleRect = mController.getViewport();

        mX.setPageLength(pageSize.width);
        mX.viewportPos = visibleRect.left;
        mX.setViewportLength(visibleRect.width());

        mY.setPageLength(pageSize.height);
        mY.viewportPos = visibleRect.top;
        mY.setViewportLength(visibleRect.height());
    }

    // The callback that performs the fling animation.
    private class FlingRunnable implements Runnable {
        public void run() {
            mX.advanceFling(); mY.advanceFling();

            if (!mOverridePanning) {
                // If both X and Y axes are overscrolled, we have to wait until both axes have stopped
                // to snap back to avoid a jarring effect.
                boolean waitingToSnapX = mX.getFlingState() == Axis.FlingStates.WAITING_TO_SNAP;
                boolean waitingToSnapY = mY.getFlingState() == Axis.FlingStates.WAITING_TO_SNAP;
                if ((mX.getOverscroll() == Axis.Overscroll.PLUS || mX.getOverscroll() == Axis.Overscroll.MINUS) &&
                    (mY.getOverscroll() == Axis.Overscroll.PLUS || mY.getOverscroll() == Axis.Overscroll.MINUS))
                {
                    if (waitingToSnapX && waitingToSnapY) {
                        mX.startSnap(); mY.startSnap();
                    }
                } else {
                    if (waitingToSnapX)
                        mX.startSnap();
                    if (waitingToSnapY)
                        mY.startSnap();
                }
            }

            mX.displace(); mY.displace();
            updatePosition();

            if (mX.getFlingState() == Axis.FlingStates.STOPPED &&
                    mY.getFlingState() == Axis.FlingStates.STOPPED) {
                stop();
            }
        }

        private void stop() {
            mState = PanZoomState.NOTHING;

            if (mFlingTimer != null) {
                mFlingTimer.cancel();
                mFlingTimer = null;
            }

            // Force a viewport synchronisation
            mController.setForceRedraw();
            mController.notifyLayerClientOfGeometryChange();
        }
    }

    private float computeElasticity(float excess, float viewportLength) {
        return 1.0f - excess / (viewportLength * SNAP_LIMIT);
    }

    // Physics information for one axis (X or Y).
    private static class Axis {
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
        private EaseOutAnimation mSnapAnim;     /* The animation when the page is snapping back. */

        /* These three need to be kept in sync with the layer controller. */
        private float viewportPos;
        private float mViewportLength;
        private int mScreenLength;
        private float mPageLength;

        public float displacement;
        private float mSnapPosition;

        public FlingStates getFlingState() { return mFlingState; }

        public void setFlingState(FlingStates aFlingState) {
            mFlingState = aFlingState;
        }

        public void setViewportLength(float viewportLength) { mViewportLength = viewportLength; }
        public void setScreenLength(int screenLength) { mScreenLength = screenLength; }
        public void setPageLength(float pageLength) { mPageLength = pageLength; }

        private float getViewportEnd() { return viewportPos + mViewportLength; }

        public Overscroll getOverscroll() {
            boolean minus = (viewportPos < 0.0f);
            boolean plus = (getViewportEnd() > mPageLength);
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
        private float getExcess() {
            switch (getOverscroll()) {
            case MINUS:     return Math.min(-viewportPos, mPageLength - getViewportEnd());
            case PLUS:      return Math.min(viewportPos, getViewportEnd() - mPageLength);
            default:        return 0.0f;
            }
        }

        // Applies resistance along the edges when tracking.
        public void applyEdgeResistance() {
            float excess = getExcess();
            if (excess > 0.0f)
                velocity *= SNAP_LIMIT - excess / mViewportLength;
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

        // Advances a fling animation by one step.
        public void advanceFling() {
            switch (mFlingState) {
            case FLINGING:
                scroll();
                return;
            case WAITING_TO_SNAP:
                // We don't do anything until the controller switches us into the snapping state.
                return;
            case SNAPPING:
                snap();
                return;
            }
        }

        // Performs one frame of a scroll operation if applicable.
        private void scroll() {
            // If we aren't overscrolled, just apply friction.
            float excess = getExcess();
            if (disableSnap || FloatUtils.fuzzyEquals(excess, 0.0f)) {
                velocity *= FRICTION;
                if (Math.abs(velocity) < 0.1f) {
                    velocity = 0.0f;
                    setFlingState(FlingStates.STOPPED);
                }
                return;
            }

            // Otherwise, decrease the velocity linearly.
            float elasticity = 1.0f - excess / (mViewportLength * SNAP_LIMIT);
            if (getOverscroll() == Overscroll.MINUS)
                velocity = Math.min((velocity + OVERSCROLL_DECEL_RATE) * elasticity, 0.0f);
            else // must be Overscroll.PLUS
                velocity = Math.max((velocity - OVERSCROLL_DECEL_RATE) * elasticity, 0.0f);

            if (Math.abs(velocity) < 0.3f) {
                velocity = 0.0f;
                setFlingState(FlingStates.WAITING_TO_SNAP);
            }
        }

        // Starts a snap-into-place operation.
        public void startSnap() {
            switch (getOverscroll()) {
            case MINUS:
                mSnapAnim = new EaseOutAnimation(0, getExcess());
                break;
            case PLUS:
                mSnapAnim = new EaseOutAnimation(0, -getExcess());
                break;
            default:
                // no overscroll to deal with, so we're done
                setFlingState(FlingStates.STOPPED);
                return;
            }

            displacement = 0;
            mSnapPosition = mSnapAnim.getPosition();
            setFlingState(FlingStates.SNAPPING);
        }

        // Performs one frame of a snap-into-place operation.
        private void snap() {
            mSnapAnim.advance();
            displacement += mSnapAnim.getPosition() - mSnapPosition;
            mSnapPosition = mSnapAnim.getPosition();

            if (mSnapAnim.getFinished()) {
                mSnapAnim = null;
                setFlingState(FlingStates.STOPPED);
            }
        }

        // Performs displacement of the viewport position according to the current velocity.
        public void displace() {
            if (locked)
                return;

            if (mFlingState == FlingStates.PANNING)
                displacement += lastTouchPos - touchPos;
            else
                displacement += velocity;
        }
    }

    private static class EaseOutAnimation {
        private float[] mFrames;
        private float mPosition;
        private float mOrigin;
        private float mDest;
        private long mTimestamp;
        private boolean mFinished;

        public EaseOutAnimation(float position, float dest) {
            mPosition = mOrigin = position;
            mDest = dest;
            mFrames = new float[SNAP_TIME];
            mTimestamp = System.currentTimeMillis();
            mFinished = false;
            plot(position, dest, mFrames);
        }

        public float getPosition() { return mPosition; }
        public boolean getFinished() { return mFinished; }

        private void advance() {
            int frame = (int)(System.currentTimeMillis() - mTimestamp);
            if (frame >= SNAP_TIME) {
                mPosition = mDest;
                mFinished = true;
                return;
            }

            mPosition = mFrames[frame];
        }

        private static void plot(float from, float to, float[] frames) {
            int nextX = 0;
            for (int i = 0; i < SUBDIVISION_COUNT; i++) {
                float t = (float)i / (float)SUBDIVISION_COUNT;
                float xPos = (3.0f*t*t - 2.0f*t*t*t) * (float)frames.length;
                if ((int)xPos < nextX)
                    continue;

                int oldX = nextX;
                nextX = (int)xPos;

                float yPos = 1.74f*t*t - 0.74f*t*t*t;
                float framePos = from + (to - from) * yPos;

                while (oldX < nextX)
                    frames[oldX++] = framePos;

                if (nextX >= frames.length)
                    break;
            }

            // Pad out any remaining frames.
            while (nextX < frames.length) {
                frames[nextX] = frames[nextX - 1];
                nextX++;
            }
        }
    }

    /*
     * Zooming
     */
    @Override
    public boolean onScale(ScaleGestureDetector detector) {
        if (mState == PanZoomState.ANIMATED_ZOOM)
            return false;

        float newZoomFactor = mController.getZoomFactor() *
                              (detector.getCurrentSpan() / detector.getPreviousSpan());

        mController.scrollBy(new PointF(mLastZoomFocus.x - detector.getFocusX(),
                                        mLastZoomFocus.y - detector.getFocusY()));
        mController.scaleWithFocus(newZoomFactor, new PointF(detector.getFocusX(), detector.getFocusY()));

        mLastZoomFocus.set(detector.getFocusX(), detector.getFocusY());

        return true;
    }

    @Override
    public boolean onScaleBegin(ScaleGestureDetector detector) {
        if (mState == PanZoomState.ANIMATED_ZOOM)
            return false;

        mState = PanZoomState.PINCHING;
        mLastZoomFocus = new PointF(detector.getFocusX(), detector.getFocusY());
        GeckoApp.mAppContext.hidePluginViews();
        cancelTouch();

        return true;
    }

    @Override
    public void onScaleEnd(ScaleGestureDetector detector) {
        PointF o = mController.getOrigin();
        if (mState == PanZoomState.ANIMATED_ZOOM)
            return;

        mState = PanZoomState.PANNING_HOLD_LOCKED;
        mX.firstTouchPos = mX.lastTouchPos = mX.touchPos = detector.getFocusX();
        mY.firstTouchPos = mY.lastTouchPos = mY.touchPos = detector.getFocusY();

        RectF viewport = mController.getViewport();

        FloatSize pageSize = mController.getPageSize();
        RectF pageRect = new RectF(0,0, pageSize.width, pageSize.height);

        if (!pageRect.contains(viewport)) {
            // animatedZoomTo will ensure that our destRect is within the page bounds
            animatedZoomTo(viewport);
        } else {
            // Force a viewport synchronisation
            mController.setForceRedraw();
            mController.notifyLayerClientOfGeometryChange();
            GeckoApp.mAppContext.showPluginViews();
        }
    }

    @Override
    public void onLongPress(MotionEvent motionEvent) {
        JSONObject ret = new JSONObject();
        try {
            PointF point = new PointF(motionEvent.getX(), motionEvent.getY());
            point = mController.convertViewPointToLayerPoint(point);
            if (point == null) {
                return;
            }
            ret.put("x", (int)Math.round(point.x));
            ret.put("y", (int)Math.round(point.y));
        } catch(Exception ex) {
            Log.w(LOGTAG, "Error building return: " + ex);
        }

        GeckoEvent e = new GeckoEvent("Gesture:LongPress", ret.toString());
        GeckoAppShell.sendEventToGecko(e);
    }

    public boolean getRedrawHint() {
        return (mState != PanZoomState.PINCHING);
    }

    @Override
    public boolean onDown(MotionEvent motionEvent) {
        JSONObject ret = new JSONObject();
        try {
            PointF point = new PointF(motionEvent.getX(), motionEvent.getY());
            point = mController.convertViewPointToLayerPoint(point);
            ret.put("x", (int)Math.round(point.x));
            ret.put("y", (int)Math.round(point.y));
        } catch(Exception ex) {
            throw new RuntimeException(ex);
        }

        GeckoEvent e = new GeckoEvent("Gesture:ShowPress", ret.toString());
        GeckoAppShell.sendEventToGecko(e);
        return false;
    }

    @Override
    public boolean onSingleTapConfirmed(MotionEvent motionEvent) {
        JSONObject ret = new JSONObject();
        try {
            PointF point = new PointF(motionEvent.getX(), motionEvent.getY());
            point = mController.convertViewPointToLayerPoint(point);
            ret.put("x", (int)Math.round(point.x));
            ret.put("y", (int)Math.round(point.y));
        } catch(Exception ex) {
            throw new RuntimeException(ex);
        }

        GeckoEvent e = new GeckoEvent("Gesture:SingleTap", ret.toString());
        GeckoAppShell.sendEventToGecko(e);
        return true;
    }

    private void cancelTouch() {
        GeckoEvent e = new GeckoEvent("Gesture:CancelTouch", "");
        GeckoAppShell.sendEventToGecko(e);
    }

    @Override
    public boolean onDoubleTap(MotionEvent motionEvent) {
        JSONObject ret = new JSONObject();
        try {
            PointF point = new PointF(motionEvent.getX(), motionEvent.getY());
            point = mController.convertViewPointToLayerPoint(point);
            ret.put("x", (int)Math.round(point.x));
            ret.put("y", (int)Math.round(point.y));
        } catch(Exception ex) {
            throw new RuntimeException(ex);
        }

        GeckoEvent e = new GeckoEvent("Gesture:DoubleTap", ret.toString());
        GeckoAppShell.sendEventToGecko(e);
        return true;
    }

    private Timer mZoomTimer;
    public boolean animatedZoomTo(RectF zoomToRect) {
        GeckoApp.mAppContext.hidePluginViews();

        if (mZoomTimer != null) {
            mZoomTimer.cancel();
        }

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
        final float finalZoom = viewport.width() * startZoom / zoomToRect.width();
        zoomToRect = RectUtils.scale(zoomToRect, finalZoom/startZoom);
        final PointF finalPoint = new PointF(zoomToRect.left, zoomToRect.top);

        mZoomTimer = new Timer();
        final long startTime = new Date().getTime();

        mZoomTimer.scheduleAtFixedRate(new TimerTask() {
            public void run() {
                long now = new Date().getTime();
                final float dt = (float)(now - startTime)/ZOOM_DURATION;

                if (dt < 1) {
                    mController.post(new Runnable() {
                        public void run() {
                            PointF currentPoint = PointUtils.interpolate(finalPoint, startPoint, dt);
                            float  currentScale = startZoom + (finalZoom-startZoom)*dt;
                            mController.scaleWithOrigin(currentScale, currentPoint);
                        }
                    });
                } else {
                    mController.post(new Runnable() {
                        public void run() {
                            mController.scaleWithOrigin(finalZoom, finalPoint);
                            mController.setForceRedraw();
                            GeckoApp.mAppContext.showPluginViews();
                            mController.notifyLayerClientOfGeometryChange();
                            populatePositionAndLength();
                        }
                    });
                    mZoomTimer.cancel();
                    mZoomTimer = null;
                    mState = PanZoomState.NOTHING;
               }
            }
        }, 0, 1000L/60L);
        return true;
    }
}
