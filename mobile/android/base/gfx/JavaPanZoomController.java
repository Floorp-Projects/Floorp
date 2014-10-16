/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.json.JSONObject;
import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.PrefsHelper;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.ZoomConstraints;
import org.mozilla.gecko.util.FloatUtils;
import org.mozilla.gecko.util.GamepadUtils;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.ThreadUtils;

import android.graphics.PointF;
import android.graphics.RectF;
import android.util.FloatMath;
import android.util.Log;
import android.view.GestureDetector;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;

/*
 * Handles the kinetic scrolling and zooming physics for a layer controller.
 *
 * Many ideas are from Joe Hewitt's Scrollability:
 *   https://github.com/joehewitt/scrollability/
 */
class JavaPanZoomController
    extends GestureDetector.SimpleOnGestureListener
    implements PanZoomController, SimpleScaleGestureDetector.SimpleScaleGestureListener, GeckoEventListener
{
    private static final String LOGTAG = "GeckoPanZoomController";

    private static final String MESSAGE_ZOOM_RECT = "Browser:ZoomToRect";
    private static final String MESSAGE_ZOOM_PAGE = "Browser:ZoomToPageWidth";
    private static final String MESSAGE_TOUCH_LISTENER = "Tab:HasTouchListener";

    // Animation stops if the velocity is below this value when overscrolled or panning.
    private static final float STOPPED_THRESHOLD = 4.0f;

    // Animation stops is the velocity is below this threshold when flinging.
    private static final float FLING_STOPPED_THRESHOLD = 0.1f;

    // The distance the user has to pan before we recognize it as such (e.g. to avoid 1-pixel pans
    // between the touch-down and touch-up of a click). In units of density-independent pixels.
    public static final float PAN_THRESHOLD = 1/16f * GeckoAppShell.getDpi();

    // Angle from axis within which we stay axis-locked
    private static final double AXIS_LOCK_ANGLE = Math.PI / 6.0; // 30 degrees

    // Axis-lock breakout angle
    private static final double AXIS_BREAKOUT_ANGLE = Math.PI / 8.0;

    // The distance the user has to pan before we consider breaking out of a locked axis
    public static final float AXIS_BREAKOUT_THRESHOLD = 1/32f * GeckoAppShell.getDpi();

    // The maximum amount we allow you to zoom into a page
    private static final float MAX_ZOOM = 4.0f;

    // The maximum amount we would like to scroll with the mouse
    private static final float MAX_SCROLL = 0.075f * GeckoAppShell.getDpi();

    // The maximum zoom factor adjustment per frame of the AUTONAV animation
    private static final float MAX_ZOOM_DELTA = 0.125f;

    // The duration of the bounce animation in ns
    private static final int BOUNCE_ANIMATION_DURATION = 250000000;

    private enum PanZoomState {
        NOTHING,                /* no touch-start events received */
        FLING,                  /* all touches removed, but we're still scrolling page */
        TOUCHING,               /* one touch-start event received */
        PANNING_LOCKED_X,       /* touch-start followed by move (i.e. panning with axis lock) X axis */
        PANNING_LOCKED_Y,       /* as above for Y axis */
        PANNING,                /* panning without axis lock */
        PANNING_HOLD,           /* in panning, but not moving.
                                 * similar to TOUCHING but after starting a pan */
        PANNING_HOLD_LOCKED_X,  /* like PANNING_HOLD, but axis lock still in effect for X axis */
        PANNING_HOLD_LOCKED_Y,  /* as above but for Y axis */
        PINCHING,               /* nth touch-start, where n > 1. this mode allows pan and zoom */
        ANIMATED_ZOOM,          /* animated zoom to a new rect */
        BOUNCE,                 /* in a bounce animation */
        WAITING_LISTENERS,      /* a state halfway between NOTHING and TOUCHING - the user has
                                   put a finger down, but we don't yet know if a touch listener has
                                   prevented the default actions yet. we still need to abort animations. */
        AUTONAV,                /* We are scrolling using an AutonavRunnable animation. This is similar
                                   to the FLING state except that it must be stopped manually by the code that
                                   started it, and it's velocity can be updated while it's running. */
    }

    private enum AxisLockMode {
        STANDARD,       /* Default axis locking mode that doesn't break out until finger release */
        FREE,           /* No locking at all */
        STICKY          /* Break out with hysteresis so that it feels as free as possible whilst locking */
    }

    private final PanZoomTarget mTarget;
    private final SubdocumentScrollHelper mSubscroller;
    private final Axis mX;
    private final Axis mY;
    private final TouchEventHandler mTouchEventHandler;
    private final EventDispatcher mEventDispatcher;

    /* The task that handles flings, autonav or bounces. */
    private PanZoomRenderTask mAnimationRenderTask;
    /* The zoom focus at the first zoom event (in page coordinates). */
    private PointF mLastZoomFocus;
    /* The time the last motion event took place. */
    private long mLastEventTime;
    /* Current state the pan/zoom UI is in. */
    private PanZoomState mState;
    /* The per-frame zoom delta for the currently-running AUTONAV animation. */
    private float mAutonavZoomDelta;
    /* The user selected panning mode */
    private AxisLockMode mMode;
    /* A medium-length tap/press is happening */
    private boolean mMediumPress;
    /* Used to change the scrollY direction */
    private boolean mNegateWheelScrollY;
    /* Whether the current event has been default-prevented. */
    private boolean mDefaultPrevented;

    // Handler to be notified when overscroll occurs
    private Overscroll mOverscroll;

    public JavaPanZoomController(PanZoomTarget target, View view, EventDispatcher eventDispatcher) {
        mTarget = target;
        mSubscroller = new SubdocumentScrollHelper(eventDispatcher);
        mX = new AxisX(mSubscroller);
        mY = new AxisY(mSubscroller);
        mTouchEventHandler = new TouchEventHandler(view.getContext(), view, this);

        checkMainThread();

        setState(PanZoomState.NOTHING);

        mEventDispatcher = eventDispatcher;
        mEventDispatcher.registerGeckoThreadListener(this,
            MESSAGE_ZOOM_RECT,
            MESSAGE_ZOOM_PAGE,
            MESSAGE_TOUCH_LISTENER);

        mMode = AxisLockMode.STANDARD;

        String[] prefs = { "ui.scrolling.axis_lock_mode",
                           "ui.scrolling.negate_wheel_scrollY",
                           "ui.scrolling.gamepad_dead_zone" };
        PrefsHelper.getPrefs(prefs, new PrefsHelper.PrefHandlerBase() {
            @Override public void prefValue(String pref, String value) {
                if (pref.equals("ui.scrolling.axis_lock_mode")) {
                    if (value.equals("standard")) {
                        mMode = AxisLockMode.STANDARD;
                    } else if (value.equals("free")) {
                        mMode = AxisLockMode.FREE;
                    } else {
                        mMode = AxisLockMode.STICKY;
                    }
                }
            }

            @Override public void prefValue(String pref, int value) {
                if (pref.equals("ui.scrolling.gamepad_dead_zone")) {
                    GamepadUtils.overrideDeadZoneThreshold(value / 1000f);
                }
            }

            @Override public void prefValue(String pref, boolean value) {
                if (pref.equals("ui.scrolling.negate_wheel_scrollY")) {
                    mNegateWheelScrollY = value;
                }
            }

            @Override
            public boolean isObserver() {
                return true;
            }

        });

        Axis.initPrefs();
    }

    @Override
    public void destroy() {
        mEventDispatcher.unregisterGeckoThreadListener(this,
            MESSAGE_ZOOM_RECT,
            MESSAGE_ZOOM_PAGE,
            MESSAGE_TOUCH_LISTENER);
        mSubscroller.destroy();
        mTouchEventHandler.destroy();
    }

    private final static float easeOut(float t) {
        // ease-out approx.
        // -(t-1)^2+1
        t = t-1;
        return -t*t+1;
    }

    private void setState(PanZoomState state) {
        if (state != mState) {
            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("PanZoom:StateChange", state.toString()));
            mState = state;

            // Let the target know we've finished with it (for now)
            if (state == PanZoomState.NOTHING) {
                mTarget.panZoomStopped();
            }
        }
    }

    private ImmutableViewportMetrics getMetrics() {
        return mTarget.getViewportMetrics();
    }

    private void checkMainThread() {
        if (!ThreadUtils.isOnUiThread()) {
            // log with full stack trace
            Log.e(LOGTAG, "Uh-oh, we're running on the wrong thread!", new Exception());
        }
    }

    @Override
    public void handleMessage(String event, JSONObject message) {
        try {
            if (MESSAGE_ZOOM_RECT.equals(event)) {
                float x = (float)message.getDouble("x");
                float y = (float)message.getDouble("y");
                final RectF zoomRect = new RectF(x, y,
                                     x + (float)message.getDouble("w"),
                                     y + (float)message.getDouble("h"));
                if (message.optBoolean("animate", true)) {
                    mTarget.post(new Runnable() {
                        @Override
                        public void run() {
                            animatedZoomTo(zoomRect);
                        }
                    });
                } else {
                    mTarget.setViewportMetrics(getMetricsToZoomTo(zoomRect));
                }
            } else if (MESSAGE_ZOOM_PAGE.equals(event)) {
                ImmutableViewportMetrics metrics = getMetrics();
                RectF cssPageRect = metrics.getCssPageRect();

                RectF viewableRect = metrics.getCssViewport();
                float y = viewableRect.top;
                // attempt to keep zoom keep focused on the center of the viewport
                float newHeight = viewableRect.height() * cssPageRect.width() / viewableRect.width();
                float dh = viewableRect.height() - newHeight; // increase in the height
                final RectF r = new RectF(0.0f,
                                    y + dh/2,
                                    cssPageRect.width(),
                                    y + dh/2 + newHeight);
                if (message.optBoolean("animate", true)) {
                    mTarget.post(new Runnable() {
                        @Override
                        public void run() {
                            animatedZoomTo(r);
                        }
                    });
                } else {
                    mTarget.setViewportMetrics(getMetricsToZoomTo(r));
                }
            } else if (MESSAGE_TOUCH_LISTENER.equals(event)) {
                int tabId = message.getInt("tabID");
                final Tab tab = Tabs.getInstance().getTab(tabId);
                // Make sure we still have a Tab
                if (tab == null) {
                    return;
                }

                tab.setHasTouchListeners(true);
                mTarget.post(new Runnable() {
                    @Override
                    public void run() {
                        if (Tabs.getInstance().isSelectedTab(tab))
                            mTouchEventHandler.setWaitForTouchListeners(true);
                    }
                });
            }
        } catch (Exception e) {
            Log.e(LOGTAG, "Exception handling message \"" + event + "\":", e);
        }
    }

    /** This function MUST be called on the UI thread */
    @Override
    public boolean onKeyEvent(KeyEvent event) {
        if (Versions.preHCMR1) {
            return false;
        }

        if ((event.getSource() & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD
            && event.getAction() == KeyEvent.ACTION_DOWN) {

            switch (event.getKeyCode()) {
            case KeyEvent.KEYCODE_ZOOM_IN:
                return animatedScale(0.2f);
            case KeyEvent.KEYCODE_ZOOM_OUT:
                return animatedScale(-0.2f);
            }
        }
        return false;
    }

    /** This function MUST be called on the UI thread */
    @Override
    public boolean onMotionEvent(MotionEvent event) {
        if (Versions.preHCMR1) {
            return false;
        }

        switch (event.getSource() & InputDevice.SOURCE_CLASS_MASK) {
        case InputDevice.SOURCE_CLASS_POINTER:
            switch (event.getAction() & MotionEvent.ACTION_MASK) {
            case MotionEvent.ACTION_SCROLL: return handlePointerScroll(event);
            }
            break;
        case InputDevice.SOURCE_CLASS_JOYSTICK:
            switch (event.getAction() & MotionEvent.ACTION_MASK) {
            case MotionEvent.ACTION_MOVE: return handleJoystickNav(event);
            }
            break;
        }
        return false;
    }

    /** This function MUST be called on the UI thread */
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        return mTouchEventHandler.handleEvent(event);
    }

    boolean handleEvent(MotionEvent event, boolean defaultPrevented) {
        mDefaultPrevented = defaultPrevented;

        switch (event.getAction() & MotionEvent.ACTION_MASK) {
        case MotionEvent.ACTION_DOWN:   return handleTouchStart(event);
        case MotionEvent.ACTION_MOVE:   return handleTouchMove(event);
        case MotionEvent.ACTION_UP:     return handleTouchEnd(event);
        case MotionEvent.ACTION_CANCEL: return handleTouchCancel(event);
        }
        return false;
    }

    /** This function MUST be called on the UI thread */
    @Override
    public void notifyDefaultActionPrevented(boolean prevented) {
        mTouchEventHandler.handleEventListenerAction(!prevented);
    }

    /** This function must be called from the UI thread. */
    @Override
    public void abortAnimation() {
        checkMainThread();
        // this happens when gecko changes the viewport on us or if the device is rotated.
        // if that's the case, abort any animation in progress and re-zoom so that the page
        // snaps to edges. for other cases (where the user's finger(s) are down) don't do
        // anything special.
        switch (mState) {
        case FLING:
            mX.stopFling();
            mY.stopFling();
            // fall through
        case BOUNCE:
        case ANIMATED_ZOOM:
            // the zoom that's in progress likely makes no sense any more (such as if
            // the screen orientation changed) so abort it
            setState(PanZoomState.NOTHING);
            // fall through
        case NOTHING:
            // Don't do animations here; they're distracting and can cause flashes on page
            // transitions.
            synchronized (mTarget.getLock()) {
                mTarget.setViewportMetrics(getValidViewportMetrics());
                mTarget.forceRedraw(null);
            }
            break;
        }
    }

    /** This function must be called on the UI thread. */
    public void startingNewEventBlock(MotionEvent event, boolean waitingForTouchListeners) {
        checkMainThread();
        mSubscroller.cancel();
        if (waitingForTouchListeners && (event.getAction() & MotionEvent.ACTION_MASK) == MotionEvent.ACTION_DOWN) {
            // this is the first touch point going down, so we enter the pending state
            // setting the state will kill any animations in progress, possibly leaving
            // the page in overscroll
            setState(PanZoomState.WAITING_LISTENERS);
        }
    }

    /** This must be called on the UI thread. */
    @Override
    public void pageRectUpdated() {
        if (mState == PanZoomState.NOTHING) {
            synchronized (mTarget.getLock()) {
                ImmutableViewportMetrics validated = getValidViewportMetrics();
                if (!getMetrics().fuzzyEquals(validated)) {
                    // page size changed such that we are now in overscroll. snap to the
                    // the nearest valid viewport
                    mTarget.setViewportMetrics(validated);
                }
            }
        }
    }

    /*
     * Panning/scrolling
     */

    private boolean handleTouchStart(MotionEvent event) {
        // user is taking control of movement, so stop
        // any auto-movement we have going
        stopAnimationTask();

        switch (mState) {
        case ANIMATED_ZOOM:
            // We just interrupted a double-tap animation, so force a redraw in
            // case this touchstart is just a tap that doesn't end up triggering
            // a redraw
            mTarget.forceRedraw(null);
            // fall through
        case FLING:
        case AUTONAV:
        case BOUNCE:
        case NOTHING:
        case WAITING_LISTENERS:
            startTouch(event.getX(0), event.getY(0), event.getEventTime());
            return false;
        case TOUCHING:
        case PANNING:
        case PANNING_LOCKED_X:
        case PANNING_LOCKED_Y:
        case PANNING_HOLD:
        case PANNING_HOLD_LOCKED_X:
        case PANNING_HOLD_LOCKED_Y:
        case PINCHING:
            Log.e(LOGTAG, "Received impossible touch down while in " + mState);
            return false;
        }
        Log.e(LOGTAG, "Unhandled case " + mState + " in handleTouchStart");
        return false;
    }

    private boolean handleTouchMove(MotionEvent event) {

        switch (mState) {
        case FLING:
        case AUTONAV:
        case BOUNCE:
        case WAITING_LISTENERS:
            // should never happen
            Log.e(LOGTAG, "Received impossible touch move while in " + mState);
            // fall through
        case ANIMATED_ZOOM:
        case NOTHING:
            // may happen if user double-taps and drags without lifting after the
            // second tap. ignore the move if this happens.
            return false;

        case TOUCHING:
            // Don't allow panning if there is a non-root element in full-screen mode. See bug 775511 and bug 859683.
            if (mTarget.getFullScreenState() == FullScreenState.NON_ROOT_ELEMENT && !mSubscroller.scrolling()) {
                return false;
            }
            if (panDistance(event) < PAN_THRESHOLD) {
                return false;
            }
            cancelTouch();
            startPanning(event.getX(0), event.getY(0), event.getEventTime());
            track(event);
            return true;

        case PANNING_HOLD_LOCKED_X:
            setState(PanZoomState.PANNING_LOCKED_X);
            track(event);
            return true;
        case PANNING_HOLD_LOCKED_Y:
            setState(PanZoomState.PANNING_LOCKED_Y);
            // fall through
        case PANNING_LOCKED_X:
        case PANNING_LOCKED_Y:
            track(event);
            return true;

        case PANNING_HOLD:
            setState(PanZoomState.PANNING);
            // fall through
        case PANNING:
            track(event);
            return true;

        case PINCHING:
            // scale gesture listener will handle this
            return false;
        }
        Log.e(LOGTAG, "Unhandled case " + mState + " in handleTouchMove");
        return false;
    }

    private boolean handleTouchEnd(MotionEvent event) {

        switch (mState) {
        case FLING:
        case AUTONAV:
        case BOUNCE:
        case ANIMATED_ZOOM:
        case NOTHING:
            // may happen if user double-taps and drags without lifting after the
            // second tap. ignore if this happens.
            return false;

        case WAITING_LISTENERS:
            if (!mDefaultPrevented) {
              // should never happen
              Log.e(LOGTAG, "Received impossible touch end while in " + mState);
            }
            // fall through
        case TOUCHING:
            // the switch into TOUCHING might have happened while the page was
            // snapping back after overscroll. we need to finish the snap if that
            // was the case
            bounce();
            return false;

        case PANNING:
        case PANNING_LOCKED_X:
        case PANNING_LOCKED_Y:
        case PANNING_HOLD:
        case PANNING_HOLD_LOCKED_X:
        case PANNING_HOLD_LOCKED_Y:
            setState(PanZoomState.FLING);
            fling();
            return true;

        case PINCHING:
            setState(PanZoomState.NOTHING);
            return true;
        }
        Log.e(LOGTAG, "Unhandled case " + mState + " in handleTouchEnd");
        return false;
    }

    private boolean handleTouchCancel(MotionEvent event) {
        cancelTouch();

        // ensure we snap back if we're overscrolled
        bounce();
        return false;
    }

    private boolean handlePointerScroll(MotionEvent event) {
        if (mState == PanZoomState.NOTHING || mState == PanZoomState.FLING) {
            float scrollX = event.getAxisValue(MotionEvent.AXIS_HSCROLL);
            float scrollY = event.getAxisValue(MotionEvent.AXIS_VSCROLL);
            if (mNegateWheelScrollY) {
                scrollY *= -1.0;
            }
            scrollBy(scrollX * MAX_SCROLL, scrollY * MAX_SCROLL);
            bounce();
            return true;
        }
        return false;
    }

    private float filterDeadZone(MotionEvent event, int axis) {
        return (GamepadUtils.isValueInDeadZone(event, axis) ? 0 : event.getAxisValue(axis));
    }

    private float normalizeJoystickScroll(MotionEvent event, int axis) {
        return filterDeadZone(event, axis) * MAX_SCROLL;
    }

    private float normalizeJoystickZoom(MotionEvent event, int axis) {
        // negate MAX_ZOOM_DELTA so that pushing up on the stick zooms in
        return filterDeadZone(event, axis) * -MAX_ZOOM_DELTA;
    }

    // Since this event is a position-based event rather than a motion-based event, we need to
    // set up an AUTONAV animation to keep scrolling even while we don't get events.
    private boolean handleJoystickNav(MotionEvent event) {
        float velocityX = normalizeJoystickScroll(event, MotionEvent.AXIS_X);
        float velocityY = normalizeJoystickScroll(event, MotionEvent.AXIS_Y);
        float zoomDelta = normalizeJoystickZoom(event, MotionEvent.AXIS_RZ);

        if (velocityX == 0 && velocityY == 0 && zoomDelta == 0) {
            if (mState == PanZoomState.AUTONAV) {
                bounce(); // if not needed, this will automatically go to state NOTHING
                return true;
            }
            return false;
        }

        if (mState == PanZoomState.NOTHING) {
            setState(PanZoomState.AUTONAV);
            startAnimationRenderTask(new AutonavRenderTask());
        }
        if (mState == PanZoomState.AUTONAV) {
            mX.setAutoscrollVelocity(velocityX);
            mY.setAutoscrollVelocity(velocityY);
            mAutonavZoomDelta = zoomDelta;
            return true;
        }
        return false;
    }

    private void startTouch(float x, float y, long time) {
        mX.startTouch(x);
        mY.startTouch(y);
        setState(PanZoomState.TOUCHING);
        mLastEventTime = time;
    }

    private void startPanning(float x, float y, long time) {
        float dx = mX.panDistance(x);
        float dy = mY.panDistance(y);
        double angle = Math.atan2(dy, dx); // range [-pi, pi]
        angle = Math.abs(angle); // range [0, pi]

        // When the touch move breaks through the pan threshold, reposition the touch down origin
        // so the page won't jump when we start panning.
        mX.startTouch(x);
        mY.startTouch(y);
        mLastEventTime = time;

        if (mMode == AxisLockMode.STANDARD || mMode == AxisLockMode.STICKY) {
            if (!mX.scrollable() || !mY.scrollable()) {
                setState(PanZoomState.PANNING);
            } else if (angle < AXIS_LOCK_ANGLE || angle > (Math.PI - AXIS_LOCK_ANGLE)) {
                mY.setScrollingDisabled(true);
                setState(PanZoomState.PANNING_LOCKED_X);
            } else if (Math.abs(angle - (Math.PI / 2)) < AXIS_LOCK_ANGLE) {
                mX.setScrollingDisabled(true);
                setState(PanZoomState.PANNING_LOCKED_Y);
            } else {
                setState(PanZoomState.PANNING);
            }
        } else if (mMode == AxisLockMode.FREE) {
            setState(PanZoomState.PANNING);
        }
    }

    private float panDistance(MotionEvent move) {
        float dx = mX.panDistance(move.getX(0));
        float dy = mY.panDistance(move.getY(0));
        return FloatMath.sqrt(dx * dx + dy * dy);
    }

    private void track(float x, float y, long time) {
        float timeDelta = (time - mLastEventTime);
        if (FloatUtils.fuzzyEquals(timeDelta, 0)) {
            // probably a duplicate event, ignore it. using a zero timeDelta will mess
            // up our velocity
            return;
        }
        mLastEventTime = time;


        // if we're axis-locked check if the user is trying to scroll away from the lock
        if (mMode == AxisLockMode.STICKY) {
            float dx = mX.panDistance(x);
            float dy = mY.panDistance(y);
            double angle = Math.atan2(dy, dx); // range [-pi, pi]
            angle = Math.abs(angle); // range [0, pi]

            if (Math.abs(dx) > AXIS_BREAKOUT_THRESHOLD || Math.abs(dy) > AXIS_BREAKOUT_THRESHOLD) {
                if (mState == PanZoomState.PANNING_LOCKED_X) {
                    if (angle > AXIS_BREAKOUT_ANGLE && angle < (Math.PI - AXIS_BREAKOUT_ANGLE)) {
                        mY.setScrollingDisabled(false);
                        setState(PanZoomState.PANNING);
                    }
                 } else if (mState == PanZoomState.PANNING_LOCKED_Y) {
                    if (Math.abs(angle - (Math.PI / 2)) > AXIS_BREAKOUT_ANGLE) {
                        mX.setScrollingDisabled(false);
                        setState(PanZoomState.PANNING);
                    }
                }
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
                setState(PanZoomState.PANNING_HOLD);
            } else if (mState == PanZoomState.PANNING_LOCKED_X) {
                setState(PanZoomState.PANNING_HOLD_LOCKED_X);
            } else if (mState == PanZoomState.PANNING_LOCKED_Y) {
                setState(PanZoomState.PANNING_HOLD_LOCKED_Y);
            } else {
                // should never happen, but handle anyway for robustness
                Log.e(LOGTAG, "Impossible case " + mState + " when stopped in track");
                setState(PanZoomState.PANNING_HOLD);
            }
        }

        mX.startPan();
        mY.startPan();
        updatePosition();
    }

    private void scrollBy(float dx, float dy) {
        mTarget.scrollBy(dx, dy);
    }

    private void fling() {
        updatePosition();

        stopAnimationTask();

        boolean stopped = stopped();
        mX.startFling(stopped);
        mY.startFling(stopped);

        startAnimationRenderTask(new FlingRenderTask());
    }

    /* Performs a bounce-back animation to the given viewport metrics. */
    private void bounce(ImmutableViewportMetrics metrics, PanZoomState state) {
        stopAnimationTask();

        ImmutableViewportMetrics bounceStartMetrics = getMetrics();
        if (bounceStartMetrics.fuzzyEquals(metrics)) {
            setState(PanZoomState.NOTHING);
            return;
        }

        setState(state);

        // At this point we have already set mState to BOUNCE or ANIMATED_ZOOM, so
        // getRedrawHint() is returning false. This means we can safely call
        // setAnimationTarget to set the new final display port and not have it get
        // clobbered by display ports from intermediate animation frames.
        mTarget.setAnimationTarget(metrics);
        startAnimationRenderTask(new BounceRenderTask(bounceStartMetrics, metrics));
    }

    /* Performs a bounce-back animation to the nearest valid viewport metrics. */
    private void bounce() {
        bounce(getValidViewportMetrics(), PanZoomState.BOUNCE);
    }

    /* Starts the fling or bounce animation. */
    private void startAnimationRenderTask(final PanZoomRenderTask task) {
        if (mAnimationRenderTask != null) {
            Log.e(LOGTAG, "Attempted to start a new task without canceling the old one!");
            stopAnimationTask();
        }

        mAnimationRenderTask = task;
        mTarget.postRenderTask(mAnimationRenderTask);
    }

    /* Stops the fling or bounce animation. */
    private void stopAnimationTask() {
        if (mAnimationRenderTask != null) {
            mAnimationRenderTask.terminate();
            mTarget.removeRenderTask(mAnimationRenderTask);
            mAnimationRenderTask = null;
        }
    }

    private float getVelocity() {
        float xvel = mX.getRealVelocity();
        float yvel = mY.getRealVelocity();
        return FloatMath.sqrt(xvel * xvel + yvel * yvel);
    }

    @Override
    public PointF getVelocityVector() {
        return new PointF(mX.getRealVelocity(), mY.getRealVelocity());
    }

    private boolean stopped() {
        return getVelocity() < STOPPED_THRESHOLD;
    }

    PointF resetDisplacement() {
        return new PointF(mX.resetDisplacement(), mY.resetDisplacement());
    }

    private void updatePosition() {
        mX.displace();
        mY.displace();
        PointF displacement = resetDisplacement();
        if (FloatUtils.fuzzyEquals(displacement.x, 0.0f) && FloatUtils.fuzzyEquals(displacement.y, 0.0f)) {
            return;
        }
        if (mDefaultPrevented || mSubscroller.scrollBy(displacement)) {
            synchronized (mTarget.getLock()) {
                mTarget.scrollMarginsBy(displacement.x, displacement.y);
            }
        } else {
            synchronized (mTarget.getLock()) {
                scrollBy(displacement.x, displacement.y);
            }
        }
    }

    /**
     * This class is an implementation of RenderTask which enforces its implementor to run in the UI thread.
     *
     */
    private abstract class PanZoomRenderTask extends RenderTask {

        /**
         * the time when the current frame was started in ns.
         */
        protected long mCurrentFrameStartTime;
        /**
         * The current frame duration in ns.
         */
        protected long mLastFrameTimeDelta;

        private final Runnable mRunnable = new Runnable() {
            @Override
            public final void run() {
                if (mContinueAnimation) {
                    animateFrame();
                }
            }
        };

        private boolean mContinueAnimation = true;

        public PanZoomRenderTask() {
            super(false);
        }

        @Override
        protected final boolean internalRun(long timeDelta, long currentFrameStartTime) {

            mCurrentFrameStartTime = currentFrameStartTime;
            mLastFrameTimeDelta = timeDelta;

            mTarget.post(mRunnable);
            return mContinueAnimation;
        }

        /**
         * The method subclasses must override. This method is run on the UI thread thanks to internalRun
         */
        protected abstract void animateFrame();

        /**
         * Terminate the animation.
         */
        public void terminate() {
            mContinueAnimation = false;
        }
    }

    private class AutonavRenderTask extends PanZoomRenderTask {
        public AutonavRenderTask() {
            super();
        }

        @Override
        protected void animateFrame() {
            if (mState != PanZoomState.AUTONAV) {
                finishAnimation();
                return;
            }

            updatePosition();
            synchronized (mTarget.getLock()) {
                mTarget.setViewportMetrics(applyZoomDelta(getMetrics(), mAutonavZoomDelta));
            }
        }
    }

    /* The task that performs the bounce animation. */
    private class BounceRenderTask extends PanZoomRenderTask {

        /*
         * The viewport metrics that represent the start and end of the bounce-back animation,
         * respectively.
         */
        private final ImmutableViewportMetrics mBounceStartMetrics;
        private final ImmutableViewportMetrics mBounceEndMetrics;
        // How long ago this bounce was started in ns.
        private long mBounceDuration;

        BounceRenderTask(ImmutableViewportMetrics startMetrics, ImmutableViewportMetrics endMetrics) {
            super();
            mBounceStartMetrics = startMetrics;
            mBounceEndMetrics = endMetrics;
        }

        @Override
        protected void animateFrame() {
            /*
             * The pan/zoom controller might have signaled to us that it wants to abort the
             * animation by setting the state to PanZoomState.NOTHING. Handle this case and bail
             * out.
             */
            if (!(mState == PanZoomState.BOUNCE || mState == PanZoomState.ANIMATED_ZOOM)) {
                finishAnimation();
                return;
            }

            /* Perform the next frame of the bounce-back animation. */
            mBounceDuration = mCurrentFrameStartTime - getStartTime();
            if (mBounceDuration < BOUNCE_ANIMATION_DURATION) {
                advanceBounce();
                return;
            }

            /* Finally, if there's nothing else to do, complete the animation and go to sleep. */
            finishBounce();
            finishAnimation();
            setState(PanZoomState.NOTHING);
        }

        /* Performs one frame of a bounce animation. */
        private void advanceBounce() {
            synchronized (mTarget.getLock()) {
                float t = easeOut((float)mBounceDuration / BOUNCE_ANIMATION_DURATION);
                ImmutableViewportMetrics newMetrics = mBounceStartMetrics.interpolate(mBounceEndMetrics, t);
                mTarget.setViewportMetrics(newMetrics);
            }
        }

        /* Concludes a bounce animation and snaps the viewport into place. */
        private void finishBounce() {
            synchronized (mTarget.getLock()) {
                mTarget.setViewportMetrics(mBounceEndMetrics);
            }
        }
    }

    // The callback that performs the fling animation.
    private class FlingRenderTask extends PanZoomRenderTask {

        public FlingRenderTask() {
            super();
        }

        @Override
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
            boolean flingingX = mX.advanceFling(mLastFrameTimeDelta);
            boolean flingingY = mY.advanceFling(mLastFrameTimeDelta);

            boolean overscrolled = (mX.overscrolled() || mY.overscrolled());

            /* If we're still flinging in any direction, update the origin. */
            if (flingingX || flingingY) {
                updatePosition();

                /*
                 * Check to see if we're still flinging with an appreciable velocity. The threshold is
                 * higher in the case of overscroll, so we bounce back eagerly when overscrolling but
                 * coast smoothly to a stop when not. In other words, require a greater velocity to
                 * maintain the fling once we enter overscroll.
                 */
                float threshold = (overscrolled && !mSubscroller.scrolling() ? STOPPED_THRESHOLD : FLING_STOPPED_THRESHOLD);
                if (getVelocity() >= threshold) {
                    // we're still flinging
                    return;
                }

                mX.stopFling();
                mY.stopFling();
            }

            /* Perform a bounce-back animation if overscrolled. */
            if (overscrolled) {
                bounce();
            } else {
                finishAnimation();
                setState(PanZoomState.NOTHING);
            }
        }
    }

    private void finishAnimation() {
        checkMainThread();

        stopAnimationTask();

        // Force a viewport synchronisation
        mTarget.forceRedraw(null);
    }

    /* Returns the nearest viewport metrics with no overscroll visible. */
    private ImmutableViewportMetrics getValidViewportMetrics() {
        return getValidViewportMetrics(getMetrics());
    }

    private ImmutableViewportMetrics getValidViewportMetrics(ImmutableViewportMetrics viewportMetrics) {
        /* First, we adjust the zoom factor so that we can make no overscrolled area visible. */
        float zoomFactor = viewportMetrics.zoomFactor;
        RectF pageRect = viewportMetrics.getPageRect();
        RectF viewport = viewportMetrics.getViewport();

        float focusX = viewport.width() / 2.0f;
        float focusY = viewport.height() / 2.0f;

        float minZoomFactor = 0.0f;
        float maxZoomFactor = MAX_ZOOM;

        ZoomConstraints constraints = mTarget.getZoomConstraints();

        if (constraints.getMinZoom() > 0)
            minZoomFactor = constraints.getMinZoom();
        if (constraints.getMaxZoom() > 0)
            maxZoomFactor = constraints.getMaxZoom();

        if (!constraints.getAllowZoom()) {
            // If allowZoom is false, clamp to the default zoom level.
            maxZoomFactor = minZoomFactor = constraints.getDefaultZoom();
        }

        // Ensure minZoomFactor keeps the page at least as big as the viewport.
        if (pageRect.width() > 0) {
            float pageWidth = pageRect.width() +
              viewportMetrics.marginLeft +
              viewportMetrics.marginRight;
            float scaleFactor = viewport.width() / pageWidth;
            minZoomFactor = Math.max(minZoomFactor, zoomFactor * scaleFactor);
            if (viewport.width() > pageWidth)
                focusX = 0.0f;
        }
        if (pageRect.height() > 0) {
            float pageHeight = pageRect.height() +
              viewportMetrics.marginTop +
              viewportMetrics.marginBottom;
            float scaleFactor = viewport.height() / pageHeight;
            minZoomFactor = Math.max(minZoomFactor, zoomFactor * scaleFactor);
            if (viewport.height() > pageHeight)
                focusY = 0.0f;
        }

        maxZoomFactor = Math.max(maxZoomFactor, minZoomFactor);

        if (zoomFactor < minZoomFactor) {
            // if one (or both) of the page dimensions is smaller than the viewport,
            // zoom using the top/left as the focus on that axis. this prevents the
            // scenario where, if both dimensions are smaller than the viewport, but
            // by different scale factors, we end up scrolled to the end on one axis
            // after applying the scale
            PointF center = new PointF(focusX, focusY);
            viewportMetrics = viewportMetrics.scaleTo(minZoomFactor, center);
        } else if (zoomFactor > maxZoomFactor) {
            PointF center = new PointF(viewport.width() / 2.0f, viewport.height() / 2.0f);
            viewportMetrics = viewportMetrics.scaleTo(maxZoomFactor, center);
        }

        /* Now we pan to the right origin. */
        viewportMetrics = viewportMetrics.clampWithMargins();

        return viewportMetrics;
    }

    private class AxisX extends Axis {
        AxisX(SubdocumentScrollHelper subscroller) { super(subscroller); }
        @Override
        public float getOrigin() { return getMetrics().viewportRectLeft; }
        @Override
        protected float getViewportLength() { return getMetrics().getWidth(); }
        @Override
        protected float getPageStart() { return getMetrics().pageRectLeft; }
        @Override
        protected float getMarginStart() { return mTarget.getMaxMargins().left - getMetrics().marginLeft; }
        @Override
        protected float getMarginEnd() { return mTarget.getMaxMargins().right - getMetrics().marginRight; }
        @Override
        protected float getPageLength() { return getMetrics().getPageWidthWithMargins(); }
        @Override
        protected boolean marginsHidden() {
            ImmutableViewportMetrics metrics = getMetrics();
            RectF maxMargins = mTarget.getMaxMargins();
            return (metrics.marginLeft < maxMargins.left || metrics.marginRight < maxMargins.right);
        }
        @Override
        protected void overscrollFling(final float velocity) {
            if (mOverscroll != null) {
                mOverscroll.setVelocity(velocity, Overscroll.Axis.X);
            }
        }
        @Override
        protected void overscrollPan(final float distance) {
            if (mOverscroll != null) {
                mOverscroll.setDistance(distance, Overscroll.Axis.X);
            }
        }
    }

    private class AxisY extends Axis {
        AxisY(SubdocumentScrollHelper subscroller) { super(subscroller); }
        @Override
        public float getOrigin() { return getMetrics().viewportRectTop; }
        @Override
        protected float getViewportLength() { return getMetrics().getHeight(); }
        @Override
        protected float getPageStart() { return getMetrics().pageRectTop; }
        @Override
        protected float getPageLength() { return getMetrics().getPageHeightWithMargins(); }
        @Override
        protected float getMarginStart() { return mTarget.getMaxMargins().top - getMetrics().marginTop; }
        @Override
        protected float getMarginEnd() { return mTarget.getMaxMargins().bottom - getMetrics().marginBottom; }
        @Override
        protected boolean marginsHidden() {
            ImmutableViewportMetrics metrics = getMetrics();
            RectF maxMargins = mTarget.getMaxMargins();
            return (metrics.marginTop < maxMargins.top || metrics.marginBottom < maxMargins.bottom);
        }
        @Override
        protected void overscrollFling(final float velocity) {
            if (mOverscroll != null) {
                mOverscroll.setVelocity(velocity, Overscroll.Axis.Y);
            }
        }
        @Override
        protected void overscrollPan(final float distance) {
            if (mOverscroll != null) {
                mOverscroll.setDistance(distance, Overscroll.Axis.Y);
            }
        }
    }

    /*
     * Zooming
     */
    @Override
    public boolean onScaleBegin(SimpleScaleGestureDetector detector) {
        if (mState == PanZoomState.ANIMATED_ZOOM)
            return false;

        if (!mTarget.getZoomConstraints().getAllowZoom())
            return false;

        setState(PanZoomState.PINCHING);
        mLastZoomFocus = new PointF(detector.getFocusX(), detector.getFocusY());
        cancelTouch();

        GeckoAppShell.sendEventToGecko(GeckoEvent.createNativeGestureEvent(GeckoEvent.ACTION_MAGNIFY_START, mLastZoomFocus, getMetrics().zoomFactor));

        return true;
    }

    @Override
    public boolean onScale(SimpleScaleGestureDetector detector) {
        if (mTarget.getFullScreenState() != FullScreenState.NONE)
            return false;

        if (mState != PanZoomState.PINCHING)
            return false;

        float prevSpan = detector.getPreviousSpan();
        if (FloatUtils.fuzzyEquals(prevSpan, 0.0f)) {
            // let's eat this one to avoid setting the new zoom to infinity (bug 711453)
            return true;
        }

        synchronized (mTarget.getLock()) {
            float zoomFactor = getAdjustedZoomFactor(detector.getCurrentSpan() / prevSpan);
            scrollBy(mLastZoomFocus.x - detector.getFocusX(),
                     mLastZoomFocus.y - detector.getFocusY());
            mLastZoomFocus.set(detector.getFocusX(), detector.getFocusY());
            ImmutableViewportMetrics target = getMetrics().scaleTo(zoomFactor, mLastZoomFocus);

            // If overscroll is disabled, prevent zooming outside the normal document pans.
            if (mX.getOverScrollMode() == View.OVER_SCROLL_NEVER || mY.getOverScrollMode() == View.OVER_SCROLL_NEVER) {
                target = getValidViewportMetrics(target);
            }
            mTarget.setViewportMetrics(target);
        }

        GeckoEvent event = GeckoEvent.createNativeGestureEvent(GeckoEvent.ACTION_MAGNIFY, mLastZoomFocus, getMetrics().zoomFactor);
        GeckoAppShell.sendEventToGecko(event);

        return true;
    }

    private ImmutableViewportMetrics applyZoomDelta(ImmutableViewportMetrics metrics, float zoomDelta) {
        float oldZoom = metrics.zoomFactor;
        float newZoom = oldZoom + zoomDelta;
        float adjustedZoom = getAdjustedZoomFactor(newZoom / oldZoom);
        // since we don't have a particular focus to zoom to, just use the center
        PointF center = new PointF(metrics.getWidth() / 2.0f, metrics.getHeight() / 2.0f);
        metrics = metrics.scaleTo(adjustedZoom, center);
        return metrics;
    }

    private boolean animatedScale(float zoomDelta) {
        if (mState != PanZoomState.NOTHING && mState != PanZoomState.BOUNCE) {
            return false;
        }
        synchronized (mTarget.getLock()) {
            ImmutableViewportMetrics metrics = applyZoomDelta(getMetrics(), zoomDelta);
            bounce(getValidViewportMetrics(metrics), PanZoomState.BOUNCE);
        }
        return true;
    }

    private float getAdjustedZoomFactor(float zoomRatio) {
        /*
         * Apply edge resistance if we're zoomed out smaller than the page size by scaling the zoom
         * factor toward 1.0.
         */
        float resistance = Math.min(mX.getEdgeResistance(true), mY.getEdgeResistance(true));
        if (zoomRatio > 1.0f)
            zoomRatio = 1.0f + (zoomRatio - 1.0f) * resistance;
        else
            zoomRatio = 1.0f - (1.0f - zoomRatio) * resistance;

        float newZoomFactor = getMetrics().zoomFactor * zoomRatio;
        float minZoomFactor = 0.0f;
        float maxZoomFactor = MAX_ZOOM;

        ZoomConstraints constraints = mTarget.getZoomConstraints();

        if (constraints.getMinZoom() > 0)
            minZoomFactor = constraints.getMinZoom();
        if (constraints.getMaxZoom() > 0)
            maxZoomFactor = constraints.getMaxZoom();

        if (newZoomFactor < minZoomFactor) {
            // apply resistance when zooming past minZoomFactor,
            // such that it asymptotically reaches minZoomFactor / 2.0
            // but never exceeds that
            final float rate = 0.5f; // controls how quickly we approach the limit
            float excessZoom = minZoomFactor - newZoomFactor;
            excessZoom = 1.0f - (float)Math.exp(-excessZoom * rate);
            newZoomFactor = minZoomFactor * (1.0f - excessZoom / 2.0f);
        }

        if (newZoomFactor > maxZoomFactor) {
            // apply resistance when zooming past maxZoomFactor,
            // such that it asymptotically reaches maxZoomFactor + 1.0
            // but never exceeds that
            float excessZoom = newZoomFactor - maxZoomFactor;
            excessZoom = 1.0f - (float)Math.exp(-excessZoom);
            newZoomFactor = maxZoomFactor + excessZoom;
        }

        return newZoomFactor;
    }

    @Override
    public void onScaleEnd(SimpleScaleGestureDetector detector) {
        if (mState == PanZoomState.ANIMATED_ZOOM)
            return;

        // switch back to the touching state
        startTouch(detector.getFocusX(), detector.getFocusY(), detector.getEventTime());

        // Force a viewport synchronisation
        mTarget.forceRedraw(null);

        PointF point = new PointF(detector.getFocusX(), detector.getFocusY());
        GeckoEvent event = GeckoEvent.createNativeGestureEvent(GeckoEvent.ACTION_MAGNIFY_END, point, getMetrics().zoomFactor);

        if (event == null) {
            return;
        }

        GeckoAppShell.sendEventToGecko(event);
    }

    @Override
    public boolean getRedrawHint() {
        switch (mState) {
            case PINCHING:
            case ANIMATED_ZOOM:
            case BOUNCE:
                // don't redraw during these because the zoom is (or might be, in the case
                // of BOUNCE) be changing rapidly and gecko will have to redraw the entire
                // display port area. we trigger a force-redraw upon exiting these states.
                return false;
            default:
                // allow redrawing in other states
                return true;
        }
    }

    private void sendPointToGecko(String event, MotionEvent motionEvent) {
        String json;
        try {
            PointF point = new PointF(motionEvent.getX(), motionEvent.getY());
            point = mTarget.convertViewPointToLayerPoint(point);
            if (point == null) {
                return;
            }
            json = PointUtils.toJSON(point).toString();
        } catch (Exception e) {
            Log.e(LOGTAG, "Unable to convert point to JSON for " + event, e);
            return;
        }

        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent(event, json));
    }

    @Override
    public boolean onDown(MotionEvent motionEvent) {
        mMediumPress = false;
        return false;
    }

    @Override
    public void onShowPress(MotionEvent motionEvent) {
        // If we get this, it will be followed either by a call to
        // onSingleTapUp (if the user lifts their finger before the
        // long-press timeout) or a call to onLongPress (if the user
        // does not). In the former case, we want to make sure it is
        // treated as a click. (Note that if this is called, we will
        // not get a call to onDoubleTap).
        mMediumPress = true;
    }

    @Override
    public void onLongPress(MotionEvent motionEvent) {
        GeckoEvent e = GeckoEvent.createLongPressEvent(motionEvent);
        GeckoAppShell.sendEventToGecko(e);
    }

    private boolean waitForDoubleTap() {
        return !mMediumPress && mTarget.getZoomConstraints().getAllowDoubleTapZoom();
    }

    @Override
    public boolean onSingleTapUp(MotionEvent motionEvent) {
        // When double-tapping is allowed, we have to wait to see if this is
        // going to be a double-tap.
        // However, if mMediumPress is true then we know there will be no
        // double-tap so we treat this as a click.
        if (!waitForDoubleTap()) {
            sendPointToGecko("Gesture:SingleTap", motionEvent);
        }
        // return false because we still want to get the ACTION_UP event that triggers this
        return false;
    }

    @Override
    public boolean onSingleTapConfirmed(MotionEvent motionEvent) {
        // In cases where we don't wait for double-tap, we handle this in onSingleTapUp.
        if (waitForDoubleTap()) {
            sendPointToGecko("Gesture:SingleTap", motionEvent);
        }
        return true;
    }

    @Override
    public boolean onDoubleTap(MotionEvent motionEvent) {
        sendPointToGecko("Gesture:DoubleTap", motionEvent);
        return true;
    }

    private void cancelTouch() {
        GeckoEvent e = GeckoEvent.createBroadcastEvent("Gesture:CancelTouch", "");
        GeckoAppShell.sendEventToGecko(e);
    }

    /**
     * Zoom to a specified rect IN CSS PIXELS.
     *
     * While we usually use device pixels, @zoomToRect must be specified in CSS
     * pixels.
     */
    private ImmutableViewportMetrics getMetricsToZoomTo(RectF zoomToRect) {
        final float startZoom = getMetrics().zoomFactor;

        RectF viewport = getMetrics().getViewport();
        // 1. adjust the aspect ratio of zoomToRect to match that of the current viewport,
        // enlarging as necessary (if it gets too big, it will get shrunk in the next step).
        // while enlarging make sure we enlarge equally on both sides to keep the target rect
        // centered.
        float targetRatio = viewport.width() / viewport.height();
        float rectRatio = zoomToRect.width() / zoomToRect.height();
        if (FloatUtils.fuzzyEquals(targetRatio, rectRatio)) {
            // all good, do nothing
        } else if (targetRatio < rectRatio) {
            // need to increase zoomToRect height
            float newHeight = zoomToRect.width() / targetRatio;
            zoomToRect.top -= (newHeight - zoomToRect.height()) / 2;
            zoomToRect.bottom = zoomToRect.top + newHeight;
        } else { // targetRatio > rectRatio) {
            // need to increase zoomToRect width
            float newWidth = targetRatio * zoomToRect.height();
            zoomToRect.left -= (newWidth - zoomToRect.width()) / 2;
            zoomToRect.right = zoomToRect.left + newWidth;
        }

        float finalZoom = viewport.width() / zoomToRect.width();

        ImmutableViewportMetrics finalMetrics = getMetrics();
        finalMetrics = finalMetrics.setViewportOrigin(
            zoomToRect.left * finalMetrics.zoomFactor,
            zoomToRect.top * finalMetrics.zoomFactor);
        finalMetrics = finalMetrics.scaleTo(finalZoom, new PointF(0.0f, 0.0f));

        // 2. now run getValidViewportMetrics on it, so that the target viewport is
        // clamped down to prevent overscroll, over-zoom, and other bad conditions.
        finalMetrics = getValidViewportMetrics(finalMetrics);
        return finalMetrics;
    }

    private boolean animatedZoomTo(RectF zoomToRect) {
        bounce(getMetricsToZoomTo(zoomToRect), PanZoomState.ANIMATED_ZOOM);
        return true;
    }

    /** This function must be called from the UI thread. */
    @Override
    public void abortPanning() {
        checkMainThread();
        bounce();
    }

    @Override
    public void setOverScrollMode(int overscrollMode) {
        mX.setOverScrollMode(overscrollMode);
        mY.setOverScrollMode(overscrollMode);
    }

    @Override
    public int getOverScrollMode() {
        return mX.getOverScrollMode();
    }

    @Override
    public void setOverscrollHandler(final Overscroll handler) {
        mOverscroll = handler;
    }
}
