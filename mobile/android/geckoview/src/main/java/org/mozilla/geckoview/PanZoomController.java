/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.PrefsHelper;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.mozglue.JNIObject;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.ThreadUtils;

import android.app.UiModeManager;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.os.SystemClock;
import androidx.annotation.AnyThread;
import androidx.annotation.NonNull;
import androidx.annotation.UiThread;
import androidx.annotation.IntDef;
import android.util.Log;
import android.util.Pair;
import android.view.MotionEvent;
import android.view.InputDevice;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

import java.util.ArrayList;

@UiThread
public class PanZoomController {
    private static final String LOGTAG = "GeckoNPZC";
    private static final int EVENT_SOURCE_SCROLL = 0;
    private static final int EVENT_SOURCE_MOTION = 1;
    private static final int EVENT_SOURCE_MOUSE = 2;
    private static final String PREF_MOUSE_AS_TOUCH = "ui.android.mouse_as_touch";
    private static boolean sTreatMouseAsTouch = true;

    private final GeckoSession mSession;
    private final Rect mTempRect = new Rect();
    private boolean mAttached;
    private float mPointerScrollFactor = 64.0f;
    private long mLastDownTime;

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({SCROLL_BEHAVIOR_SMOOTH, SCROLL_BEHAVIOR_AUTO})
    /* package */ @interface ScrollBehaviorType {}

    /**
     * Specifies smooth scrolling which animates content to the desired scroll position.
     */
    public static final int SCROLL_BEHAVIOR_SMOOTH = 0;
    /**
     * Specifies auto scrolling which jumps content to the desired scroll position.
     */
    public static final int SCROLL_BEHAVIOR_AUTO = 1;

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({INPUT_RESULT_UNHANDLED, INPUT_RESULT_HANDLED, INPUT_RESULT_HANDLED_CONTENT, INPUT_RESULT_IGNORED})
    /* package */ @interface InputResult {}

    /**
     * Specifies that an input event was not handled by the PanZoomController for a panning
     * or zooming operation. The event may have been handled by Web content or
     * internally (e.g. text selection).
     */
    @WrapForJNI
    public static final int INPUT_RESULT_UNHANDLED = 0;

    /**
     * Specifies that an input event was handled by the PanZoomController for a
     * panning or zooming operation, but likely not by any touch event listeners in Web content.
     */
    @WrapForJNI
    public static final int INPUT_RESULT_HANDLED = 1;

    /**
     * Specifies that an input event was handled by the PanZoomController and passed on
     * to touch event listeners in Web content.
     */
    @WrapForJNI
    public static final int INPUT_RESULT_HANDLED_CONTENT = 2;

    /**
     * Specifies that an input event was consumed by a PanZoomController internally and
     * browsers should do nothing in response to the event.
     */
    @WrapForJNI
    public static final int INPUT_RESULT_IGNORED = 3;

    @Retention(RetentionPolicy.SOURCE)
    @IntDef(flag = true,
            value = { SCROLLABLE_FLAG_NONE, SCROLLABLE_FLAG_TOP, SCROLLABLE_FLAG_RIGHT,
                      SCROLLABLE_FLAG_BOTTOM, SCROLLABLE_FLAG_LEFT })
    /* package */ @interface ScrollableDirections {}
    /**
     * Represents which directions can be scrolled in the scroll container where
     * an input event was handled.
     * This value is only useful in the case of {@link PanZoomController#INPUT_RESULT_HANDLED}.
     */
    /* The container cannot be scrolled. */
    @WrapForJNI
    public static final int SCROLLABLE_FLAG_NONE = 0;
    /* The container cannot be scrolled to top */
    @WrapForJNI
    public static final int SCROLLABLE_FLAG_TOP = 1 << 0;
    /* The container cannot be scrolled to right */
    @WrapForJNI
    public static final int SCROLLABLE_FLAG_RIGHT = 1 << 1;
    /* The container cannot be scrolled to bottom */
    @WrapForJNI
    public static final int SCROLLABLE_FLAG_BOTTOM = 1 << 2;
    /* The container cannot be scrolled to left */
    @WrapForJNI
    public static final int SCROLLABLE_FLAG_LEFT = 1 << 3;

    @Retention(RetentionPolicy.SOURCE)
    @IntDef(flag = true,
            value = { OVERSCROLL_FLAG_NONE, OVERSCROLL_FLAG_HORIZONTAL, OVERSCROLL_FLAG_VERTICAL })
    /* package */ @interface OverscrollDirections {}
    /**
     * Represents which directions can be over-scrolled in the scroll container where
     * an input event was handled.
     * This value is only useful in the case of {@link PanZoomController#INPUT_RESULT_HANDLED}.
     */
    /* the container cannot be over-scrolled. */
    @WrapForJNI
    public static final int OVERSCROLL_FLAG_NONE = 0;
    /* the container can be over-scrolled horizontally. */
    @WrapForJNI
    public static final int OVERSCROLL_FLAG_HORIZONTAL = 1 << 0;
    /* the container can be over-scrolled vertically. */
    @WrapForJNI
    public static final int OVERSCROLL_FLAG_VERTICAL = 1 << 1;

    /**
     * Represents how a {@link MotionEvent} was handled in Gecko.
     * This value can be used by browser apps to implement features like pull-to-refresh. Failing
     * to account this value might break some websites expectations about touch events.
     *
     * For example, a {@link PanZoomController.InputResultDetail#handledResult} value of
     * {@link PanZoomController#INPUT_RESULT_HANDLED} and
     * {@link PanZoomController.InputResultDetail#overscrollDirections} of
     * {@link PanZoomController#OVERSCROLL_FLAG_NONE} indicates that the event was consumed for a panning
     * or zooming operation and that the website does not expect the browser to react to the touch
     * event (say, by triggering the pull-to-refresh feature) even though the scroll container reached
     * to the edge.
     */
    @WrapForJNI
    public static class InputResultDetail {
        protected InputResultDetail(final @InputResult int handledResult,
                                    final @ScrollableDirections int scrollableDirections,
                                    final @OverscrollDirections int overscrollDirections) {
            mHandledResult = handledResult;
            mScrollableDirections = scrollableDirections;
            mOverscrollDirections = overscrollDirections;
        }

        /**
         * @return One of the {@link #INPUT_RESULT_UNHANDLED INPUT_RESULT_*} indicating how
         * the event was handled.
         */
        @AnyThread
        public @InputResult int handledResult() {
            return mHandledResult;
        }
        /**
         * @return an OR-ed value of {@link #SCROLLABLE_FLAG_NONE SCROLLABLE_FLAG_*} indicating which
         * directions can be scrollable.
         */
        @AnyThread
        public @ScrollableDirections int scrollableDirections() {
            return mScrollableDirections;
        }
        /**
         * @return an OR-ed value of {@link #OVERSCROLL_FLAG_NONE OVERSCROLL_FLAG_*} indicating which
         * directions can be over-scrollable.
         */
        @AnyThread
        public @OverscrollDirections int overscrollDirections() {
            return mOverscrollDirections;
        }

        private final @InputResult int mHandledResult;
        private final @ScrollableDirections int mScrollableDirections;
        private final @OverscrollDirections int mOverscrollDirections;
    }

    private SynthesizedEventState mPointerState;

    private ArrayList<Pair<Integer, MotionEvent>> mQueuedEvents;

    private boolean mSynthesizedEvent = false;

    @WrapForJNI
    private static class MotionEventData {
        public final int action;
        public final int actionIndex;
        public final long time;
        public final int metaState;
        public final int pointerId[];
        public final int historySize;
        public final long historicalTime[];
        public final float historicalX[];
        public final float historicalY[];
        public final float historicalOrientation[];
        public final float historicalPressure[];
        public final float historicalToolMajor[];
        public final float historicalToolMinor[];
        public final float x[];
        public final float y[];
        public final float orientation[];
        public final float pressure[];
        public final float toolMajor[];
        public final float toolMinor[];

        public MotionEventData(final MotionEvent event) {
            final int count = event.getPointerCount();
            action = event.getActionMasked();
            actionIndex = event.getActionIndex();
            time = event.getEventTime();
            metaState = event.getMetaState();
            historySize = event.getHistorySize();
            historicalTime = new long[historySize];
            historicalX = new float[historySize * count];
            historicalY = new float[historySize * count];
            historicalOrientation = new float[historySize * count];
            historicalPressure = new float[historySize * count];
            historicalToolMajor = new float[historySize * count];
            historicalToolMinor = new float[historySize * count];
            pointerId = new int[count];
            x = new float[count];
            y = new float[count];
            orientation = new float[count];
            pressure = new float[count];
            toolMajor = new float[count];
            toolMinor = new float[count];


            for (int historyIndex = 0; historyIndex < historySize; historyIndex++) {
                historicalTime[historyIndex] = event.getHistoricalEventTime(historyIndex);
            }

            final MotionEvent.PointerCoords coords = new MotionEvent.PointerCoords();
            for (int i = 0; i < count; i++) {
                pointerId[i] = event.getPointerId(i);

                for (int historyIndex = 0; historyIndex < historySize; historyIndex++) {
                    event.getHistoricalPointerCoords(i, historyIndex, coords);

                    final int historicalI = historyIndex * count + i;
                    historicalX[historicalI] = coords.x;
                    historicalY[historicalI] = coords.y;

                    historicalOrientation[historicalI] = coords.orientation;
                    historicalPressure[historicalI] = coords.pressure;

                    // If we are converting to CSS pixels, we should adjust the radii as well.
                    historicalToolMajor[historicalI] = coords.toolMajor;
                    historicalToolMinor[historicalI] = coords.toolMinor;
                }

                event.getPointerCoords(i, coords);

                x[i] = coords.x;
                y[i] = coords.y;

                orientation[i] = coords.orientation;
                pressure[i] = coords.pressure;

                // If we are converting to CSS pixels, we should adjust the radii as well.
                toolMajor[i] = coords.toolMajor;
                toolMinor[i] = coords.toolMinor;
            }
        }
    }

    /* package */ final class NativeProvider extends JNIObject {
        @Override // JNIObject
        protected void disposeNative() {
            // Disposal happens in native code.
            throw new UnsupportedOperationException();
        }

        @WrapForJNI(calledFrom = "ui")
        private native void handleMotionEvent(
               MotionEventData eventData, float screenX, float screenY,
               GeckoResult<InputResultDetail> result);

        @WrapForJNI(calledFrom = "ui")
        private native @InputResult int handleScrollEvent(
                long time, int metaState,
                float x, float y,
                float hScroll, float vScroll);

        @WrapForJNI(calledFrom = "ui")
        private native @InputResult int handleMouseEvent(
                int action, long time, int metaState,
                float x, float y, int buttons);

        @WrapForJNI(stubName = "SetIsLongpressEnabled") // Called from test thread.
        private native void nativeSetIsLongpressEnabled(boolean isLongpressEnabled);

        @WrapForJNI(calledFrom = "ui")
        private void synthesizeNativeTouchPoint(final int pointerId, final int eventType,
                                                final int clientX, final int clientY,
                                                final double pressure, final int orientation) {
            if (pointerId == PointerInfo.RESERVED_MOUSE_POINTER_ID) {
                throw new IllegalArgumentException("Pointer ID reserved for mouse");
            }
            synthesizeNativePointer(InputDevice.SOURCE_TOUCHSCREEN, pointerId,
                    eventType, clientX, clientY, pressure, orientation, 0);
        }

        @WrapForJNI(calledFrom = "ui")
        private void synthesizeNativeMouseEvent(final int eventType, final int clientX,
                                                final int clientY, final int button) {
            synthesizeNativePointer(InputDevice.SOURCE_MOUSE,
                    PointerInfo.RESERVED_MOUSE_POINTER_ID,
                    eventType, clientX, clientY, 0, 0, button);
        }

        @WrapForJNI(calledFrom = "ui")
        private void setAttached(final boolean attached) {
            if (attached) {
                mAttached = true;
                flushEventQueue();
            } else if (mAttached) {
                mAttached = false;
                enableEventQueue();
            }
        }
    }

    /* package */ final NativeProvider mNative = new NativeProvider();

    private void handleMotionEvent(final MotionEvent event) {
        handleMotionEvent(event, null);
    }

    private void handleMotionEvent(final MotionEvent event, final GeckoResult<InputResultDetail> result) {
        if (!mAttached) {
            mQueuedEvents.add(new Pair<>(EVENT_SOURCE_MOTION, event));
            if (result != null) {
                result.complete(
                    new InputResultDetail(INPUT_RESULT_HANDLED, SCROLLABLE_FLAG_NONE, OVERSCROLL_FLAG_NONE));
            }
            return;
        }

        final int action = event.getActionMasked();

        if (action == MotionEvent.ACTION_DOWN) {
            mLastDownTime = event.getDownTime();
        } else if (mLastDownTime != event.getDownTime()) {
            if (result != null) {
                result.complete(
                    new InputResultDetail(INPUT_RESULT_UNHANDLED, SCROLLABLE_FLAG_NONE, OVERSCROLL_FLAG_NONE));
            }
            return;
        }

        final float screenX = event.getRawX() - event.getX();
        final float screenY = event.getRawY() - event.getY();

        // Take this opportunity to update screen origin of session. This gets
        // dispatched to the gecko thread, so we also pass the new screen x/y directly to apz.
        // If this is a synthesized touch, the screen offset is bogus so ignore it.
        if (!mSynthesizedEvent) {
            mSession.onScreenOriginChanged((int)screenX, (int)screenY);
        }

        final MotionEventData data = new MotionEventData(event);
        mNative.handleMotionEvent(data, screenX, screenY, result);
    }

    private @InputResult int handleScrollEvent(final MotionEvent event) {
        if (!mAttached) {
            mQueuedEvents.add(new Pair<>(EVENT_SOURCE_SCROLL, event));
            return INPUT_RESULT_HANDLED;
        }

        final int count = event.getPointerCount();

        if (count <= 0) {
            return INPUT_RESULT_UNHANDLED;
        }

        final MotionEvent.PointerCoords coords = new MotionEvent.PointerCoords();
        event.getPointerCoords(0, coords);

        // Translate surface origin to client origin for scroll events.
        mSession.getSurfaceBounds(mTempRect);
        final float x = coords.x - mTempRect.left;
        final float y = coords.y - mTempRect.top;

        final float hScroll = event.getAxisValue(MotionEvent.AXIS_HSCROLL) *
                              mPointerScrollFactor;
        final float vScroll = event.getAxisValue(MotionEvent.AXIS_VSCROLL) *
                              mPointerScrollFactor;

        return mNative.handleScrollEvent(event.getEventTime(), event.getMetaState(), x, y,
                                         hScroll, vScroll);
    }

    private @InputResult int handleMouseEvent(final MotionEvent event) {
        if (!mAttached) {
            mQueuedEvents.add(new Pair<>(EVENT_SOURCE_MOUSE, event));
            return INPUT_RESULT_UNHANDLED;
        }

        final int count = event.getPointerCount();

        if (count <= 0) {
            return INPUT_RESULT_UNHANDLED;
        }

        final MotionEvent.PointerCoords coords = new MotionEvent.PointerCoords();
        event.getPointerCoords(0, coords);

        // Translate surface origin to client origin for mouse events.
        mSession.getSurfaceBounds(mTempRect);
        final float x = coords.x - mTempRect.left;
        final float y = coords.y - mTempRect.top;

        return mNative.handleMouseEvent(event.getActionMasked(), event.getEventTime(),
                                        event.getMetaState(), x, y, event.getButtonState());
    }

    protected PanZoomController(final GeckoSession session) {
        mSession = session;
        enableEventQueue();
        initMouseAsTouch();
    }

    private static void initMouseAsTouch() {
        final PrefsHelper.PrefHandler prefHandler = new PrefsHelper.PrefHandlerBase() {
            @Override
            public void prefValue(final String pref, final int value) {
                if (!PREF_MOUSE_AS_TOUCH.equals(pref)) {
                    return;
                }
                if (value == 0) {
                    sTreatMouseAsTouch = false;
                } else if (value == 1) {
                    sTreatMouseAsTouch = true;
                } else if (value == 2) {
                    final Context c = GeckoAppShell.getApplicationContext();
                    final UiModeManager m = (UiModeManager)c.getSystemService(Context.UI_MODE_SERVICE);
                    // on TV devices, treat mouse as touch. everywhere else, don't
                    sTreatMouseAsTouch = (m.getCurrentModeType() == Configuration.UI_MODE_TYPE_TELEVISION);
                }
            }
        };
        PrefsHelper.addObserver(new String[] { PREF_MOUSE_AS_TOUCH }, prefHandler);
        PrefsHelper.getPref(PREF_MOUSE_AS_TOUCH, prefHandler);
    }

    /**
     * Set the current scroll factor. The scroll factor is the maximum scroll amount that
     * one scroll event may generate, in device pixels.
     *
     * @param factor Scroll factor.
     */
    public void setScrollFactor(final float factor) {
        ThreadUtils.assertOnUiThread();
        mPointerScrollFactor = factor;
    }

    /**
     * Get the current scroll factor.
     *
     * @return Scroll factor.
     */
    public float getScrollFactor() {
        ThreadUtils.assertOnUiThread();
        return mPointerScrollFactor;
    }

    /**
     * This is a workaround for touch pad on Android app by Chrome OS.
     * Android app on Chrome OS fires weird motion event by two finger scroll.
     * See https://crbug.com/704051
     */
    private boolean mayTouchpadScroll(final @NonNull MotionEvent event) {
        final int action = event.getActionMasked();
        return event.getButtonState() == 0 &&
               (action == MotionEvent.ACTION_DOWN ||
                (mLastDownTime == event.getDownTime() &&
                 (action == MotionEvent.ACTION_MOVE || action == MotionEvent.ACTION_UP)));
    }

    /**
     * Process a touch event through the pan-zoom controller. Treat any mouse events as
     * "touch" rather than as "mouse". Pointer coordinates should be relative to the
     * display surface.
     *
     * @param event MotionEvent to process.
     */
    public void onTouchEvent(final @NonNull MotionEvent event) {
        ThreadUtils.assertOnUiThread();

        if (!sTreatMouseAsTouch && event.getToolType(0) == MotionEvent.TOOL_TYPE_MOUSE && !mayTouchpadScroll(event)) {
            handleMouseEvent(event);
            return;
        }
        handleMotionEvent(event);
    }

    /**
     * Process a touch event through the pan-zoom controller. Treat any mouse events as
     * "touch" rather than as "mouse". Pointer coordinates should be relative to the
     * display surface.
     *
     * NOTE: It is highly recommended to only call this with ACTION_DOWN or in otherwise
     * limited capacity. Returning a GeckoResult for every touch event will generate
     * a lot of allocations and unnecessary GC pressure. Instead, prefer to call
     * {@link #onTouchEvent(MotionEvent)}.
     *
     * @param event MotionEvent to process.
     * @return A GeckoResult resolving to one of the
     *         {@link PanZoomController#INPUT_RESULT_UNHANDLED INPUT_RESULT_*}) constants indicating
     *         how the event was handled.
     */
    @Deprecated @DeprecationSchedule(version = 90, id = "on-touch-event-for-result")
    public @NonNull GeckoResult<Integer> onTouchEventForResult(final @NonNull MotionEvent event) {
        return onTouchEventForDetailResult(event).map(detail -> detail.handledResult());
    }

    /**
     * Process a touch event through the pan-zoom controller. Treat any mouse events as
     * "touch" rather than as "mouse". Pointer coordinates should be relative to the
     * display surface.
     *
     * NOTE: It is highly recommended to only call this with ACTION_DOWN or in otherwise
     * limited capacity. Returning a GeckoResult for every touch event will generate
     * a lot of allocations and unnecessary GC pressure. Instead, prefer to call
     * {@link #onTouchEvent(MotionEvent)}.
     *
     * @param event MotionEvent to process.
     * @return A GeckoResult resolving to {@link PanZoomController.InputResultDetail}).
     */
    public @NonNull GeckoResult<InputResultDetail> onTouchEventForDetailResult(final @NonNull MotionEvent event) {
        ThreadUtils.assertOnUiThread();

        if (!sTreatMouseAsTouch && event.getToolType(0) == MotionEvent.TOOL_TYPE_MOUSE && !mayTouchpadScroll(event)) {
            return GeckoResult.fromValue(
                new InputResultDetail(handleMouseEvent(event), SCROLLABLE_FLAG_NONE, OVERSCROLL_FLAG_NONE));
        }

        final GeckoResult<InputResultDetail> result = new GeckoResult<>();
        handleMotionEvent(event, result);
        return result;
    }

    /**
     * Process a touch event through the pan-zoom controller. Treat any mouse events as
     * "mouse" rather than as "touch". Pointer coordinates should be relative to the
     * display surface.
     *
     * @param event MotionEvent to process.
     */
    public void onMouseEvent(final @NonNull MotionEvent event) {
        ThreadUtils.assertOnUiThread();

        if (event.getToolType(0) == MotionEvent.TOOL_TYPE_MOUSE) {
            return;
        }
        handleMotionEvent(event);
    }

    @Override
    protected void finalize() throws Throwable {
        mNative.setAttached(false);
    }

    /**
     * Process a non-touch motion event through the pan-zoom controller. Currently, hover
     * and scroll events are supported. Pointer coordinates should be relative to the
     * display surface.
     *
     * @param event MotionEvent to process.
     */
    public void onMotionEvent(final @NonNull MotionEvent event) {
        ThreadUtils.assertOnUiThread();

        final int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_SCROLL) {
            if (event.getDownTime() >= mLastDownTime) {
                mLastDownTime = event.getDownTime();
            } else if ((InputDevice.getDevice(event.getDeviceId()) != null) &&
                       (InputDevice.getDevice(event.getDeviceId()).getSources() &
                        InputDevice.SOURCE_TOUCHPAD) == InputDevice.SOURCE_TOUCHPAD) {
                return;
            }
            handleScrollEvent(event);
        } else if ((action == MotionEvent.ACTION_HOVER_MOVE) ||
                   (action == MotionEvent.ACTION_HOVER_ENTER) ||
                   (action == MotionEvent.ACTION_HOVER_EXIT)) {
            handleMouseEvent(event);
        }
    }

    private void enableEventQueue() {
        if (mQueuedEvents != null) {
            throw new IllegalStateException("Already have an event queue");
        }
        mQueuedEvents = new ArrayList<>();
    }

    private void flushEventQueue() {
        if (mQueuedEvents == null) {
            return;
        }

        final ArrayList<Pair<Integer, MotionEvent>> events = mQueuedEvents;
        mQueuedEvents = null;
        for (final Pair<Integer, MotionEvent> pair : events) {
            switch (pair.first) {
                case EVENT_SOURCE_MOTION:
                    handleMotionEvent(pair.second);
                    break;
                case EVENT_SOURCE_SCROLL:
                    handleScrollEvent(pair.second);
                    break;
                case EVENT_SOURCE_MOUSE:
                    handleMouseEvent(pair.second);
                    break;
            }
        }
    }

    /**
     * Set whether Gecko should generate long-press events.
     *
     * @param isLongpressEnabled True if Gecko should generate long-press events.
     */
    public void setIsLongpressEnabled(final boolean isLongpressEnabled) {
        ThreadUtils.assertOnUiThread();

        if (mAttached) {
            mNative.nativeSetIsLongpressEnabled(isLongpressEnabled);
        }
    }

    private static class PointerInfo {
        // We reserve one pointer ID for the mouse, so that tests don't have
        // to worry about tracking pointer IDs if they just want to test mouse
        // event synthesization. If somebody tries to use this ID for a
        // synthesized touch event we'll throw an exception.
        public static final int RESERVED_MOUSE_POINTER_ID = 100000;

        public int pointerId;
        public int source;
        public int surfaceX;
        public int surfaceY;
        public double pressure;
        public int orientation;
        public int buttonState;

        public MotionEvent.PointerCoords getCoords() {
            final MotionEvent.PointerCoords coords = new MotionEvent.PointerCoords();
            coords.orientation = orientation;
            coords.pressure = (float)pressure;
            coords.x = surfaceX;
            coords.y = surfaceY;
            return coords;
        }
    }

    private static class SynthesizedEventState {
        public final ArrayList<PointerInfo> pointers;
        public long downTime;

        SynthesizedEventState() {
            pointers = new ArrayList<PointerInfo>();
        }

        int getPointerIndex(final int pointerId) {
            for (int i = 0; i < pointers.size(); i++) {
                if (pointers.get(i).pointerId == pointerId) {
                    return i;
                }
            }
            return -1;
        }

        int addPointer(final int pointerId, final int source) {
            final PointerInfo info = new PointerInfo();
            info.pointerId = pointerId;
            info.source = source;
            pointers.add(info);
            return pointers.size() - 1;
        }

        int getPointerCount(final int source) {
            int count = 0;
            for (int i = 0; i < pointers.size(); i++) {
                if (pointers.get(i).source == source) {
                    count++;
                }
            }
            return count;
        }

        int getPointerButtonState(final int source) {
            for (int i = 0; i < pointers.size(); i++) {
                if (pointers.get(i).source == source) {
                    return pointers.get(i).buttonState;
                }
            }
            return 0;
        }

        MotionEvent.PointerProperties[] getPointerProperties(final int source) {
            final MotionEvent.PointerProperties[] props =
                    new MotionEvent.PointerProperties[getPointerCount(source)];
            int index = 0;
            for (int i = 0; i < pointers.size(); i++) {
                if (pointers.get(i).source == source) {
                    final MotionEvent.PointerProperties p = new MotionEvent.PointerProperties();
                    p.id = pointers.get(i).pointerId;
                    switch (source) {
                        case InputDevice.SOURCE_TOUCHSCREEN:
                            p.toolType = MotionEvent.TOOL_TYPE_FINGER;
                            break;
                        case InputDevice.SOURCE_MOUSE:
                            p.toolType = MotionEvent.TOOL_TYPE_MOUSE;
                            break;
                    }
                    props[index++] = p;
                }
            }
            return props;
        }

        MotionEvent.PointerCoords[] getPointerCoords(final int source) {
            final MotionEvent.PointerCoords[] coords =
                    new MotionEvent.PointerCoords[getPointerCount(source)];
            int index = 0;
            for (int i = 0; i < pointers.size(); i++) {
                if (pointers.get(i).source == source) {
                    coords[index++] = pointers.get(i).getCoords();
                }
            }
            return coords;
        }
    }

    private void synthesizeNativePointer(final int source, final int pointerId,
                                         final int originalEventType,
                                         final int clientX, final int clientY,
                                         final double pressure, final int orientation,
                                         final int button) {
        if (mPointerState == null) {
            mPointerState = new SynthesizedEventState();
        }

        // Find the pointer if it already exists
        int pointerIndex = mPointerState.getPointerIndex(pointerId);

        // Event-specific handling
        int eventType = originalEventType;
        switch (originalEventType) {
            case MotionEvent.ACTION_POINTER_UP:
                if (pointerIndex < 0) {
                    Log.w(LOGTAG, "Pointer-up for invalid pointer");
                    return;
                }
                if (mPointerState.pointers.size() == 1) {
                    // Last pointer is going up
                    eventType = MotionEvent.ACTION_UP;
                }
                break;
            case MotionEvent.ACTION_CANCEL:
                if (pointerIndex < 0) {
                    Log.w(LOGTAG, "Pointer-cancel for invalid pointer");
                    return;
                }
                break;
            case MotionEvent.ACTION_POINTER_DOWN:
                if (pointerIndex < 0) {
                    // Adding a new pointer
                    pointerIndex = mPointerState.addPointer(pointerId, source);
                    if (pointerIndex == 0) {
                        // first pointer
                        eventType = MotionEvent.ACTION_DOWN;
                        mPointerState.downTime = SystemClock.uptimeMillis();
                    }
                } else {
                    // We're moving an existing pointer
                    eventType = MotionEvent.ACTION_MOVE;
                }
                break;
            case MotionEvent.ACTION_HOVER_MOVE:
                if (pointerIndex < 0) {
                    // Mouse-move a pointer without it going "down". However
                    // in order to send the right MotionEvent without a lot of
                    // duplicated code, we add the pointer to mPointerState,
                    // and then remove it at the bottom of this function.
                    pointerIndex = mPointerState.addPointer(pointerId, source);
                } else {
                    // We're moving an existing mouse pointer that went down.
                    eventType = MotionEvent.ACTION_MOVE;
                }
                break;
        }

        // Translate client origin to surface origin.
        mSession.getSurfaceBounds(mTempRect);
        final int surfaceX = clientX + mTempRect.left;
        final int surfaceY = clientY + mTempRect.top;

        // Update the pointer with the new info
        final PointerInfo info = mPointerState.pointers.get(pointerIndex);
        info.surfaceX = surfaceX;
        info.surfaceY = surfaceY;
        info.pressure = pressure;
        info.orientation = orientation;
        if (source == InputDevice.SOURCE_MOUSE) {
            if (eventType == MotionEvent.ACTION_DOWN ||
                eventType == MotionEvent.ACTION_MOVE) {
                info.buttonState |= button;
            } else if (eventType == MotionEvent.ACTION_UP) {
                info.buttonState &= button;
            }
        }

        // Dispatch the event
        int action = 0;
        if (eventType == MotionEvent.ACTION_POINTER_DOWN ||
            eventType == MotionEvent.ACTION_POINTER_UP) {
            // for pointer-down and pointer-up events we need to add the
            // index of the relevant pointer.
            action = (pointerIndex << MotionEvent.ACTION_POINTER_INDEX_SHIFT);
            action &= MotionEvent.ACTION_POINTER_INDEX_MASK;
        }
        action |= (eventType & MotionEvent.ACTION_MASK);
        final MotionEvent event = MotionEvent.obtain(
            /*downTime*/ mPointerState.downTime,
            /*eventTime*/ SystemClock.uptimeMillis(),
            /*action*/ action,
            /*pointerCount*/ mPointerState.getPointerCount(source),
            /*pointerProperties*/ mPointerState.getPointerProperties(source),
            /*pointerCoords*/ mPointerState.getPointerCoords(source),
            /*metaState*/ 0,
            /*buttonState*/ mPointerState.getPointerButtonState(source),
            /*xPrecision*/ 0,
            /*yPrecision*/ 0,
            /*deviceId*/ 0,
            /*edgeFlags*/ 0,
            /*source*/ source,
            /*flags*/ 0);

        mSynthesizedEvent = true;
        onTouchEvent(event);
        mSynthesizedEvent = false;

        // Forget about removed pointers
        if (eventType == MotionEvent.ACTION_POINTER_UP ||
            eventType == MotionEvent.ACTION_UP ||
            eventType == MotionEvent.ACTION_CANCEL ||
            eventType == MotionEvent.ACTION_HOVER_MOVE) {
            mPointerState.pointers.remove(pointerIndex);
        }
    }

    /**
     * Scroll the document body by an offset from the current scroll position.
     * Uses {@link #SCROLL_BEHAVIOR_SMOOTH}.
     *
     * @param width {@link ScreenLength} offset to scroll along X axis.
     * @param height {@link ScreenLength} offset to scroll along Y axis.
     */
    @UiThread
    public void scrollBy(final @NonNull ScreenLength width, final @NonNull ScreenLength height) {
        scrollBy(width, height, SCROLL_BEHAVIOR_SMOOTH);
    }

    /**
     * Scroll the document body by an offset from the current scroll position.
     *
     * @param width {@link ScreenLength} offset to scroll along X axis.
     * @param height {@link ScreenLength} offset to scroll along Y axis.
     * @param behavior ScrollBehaviorType One of {@link #SCROLL_BEHAVIOR_SMOOTH}, {@link #SCROLL_BEHAVIOR_AUTO},
     *                 that specifies how to scroll the content.
     */
    @UiThread
    public void scrollBy(final @NonNull ScreenLength width, final @NonNull ScreenLength height,
                         final @ScrollBehaviorType int behavior) {
        final GeckoBundle msg = buildScrollMessage(width, height, behavior);
        mSession.getEventDispatcher().dispatch("GeckoView:ScrollBy", msg);
    }

    /**
     * Scroll the document body to an absolute  position.
     * Uses {@link #SCROLL_BEHAVIOR_SMOOTH}.
     *
     * @param width {@link ScreenLength} position to scroll along X axis.
     * @param height {@link ScreenLength} position to scroll along Y axis.
     */
    @UiThread
    public void scrollTo(final @NonNull ScreenLength width, final @NonNull ScreenLength height) {
        scrollTo(width, height, SCROLL_BEHAVIOR_SMOOTH);
    }

    /**
     * Scroll the document body to an absolute position.
     *
     * @param width {@link ScreenLength} position to scroll along X axis.
     * @param height {@link ScreenLength} position to scroll along Y axis.
     * @param behavior ScrollBehaviorType One of {@link #SCROLL_BEHAVIOR_SMOOTH}, {@link #SCROLL_BEHAVIOR_AUTO},
     *                 that specifies how to scroll the content.
     */
    @UiThread
    public void scrollTo(final @NonNull ScreenLength width, final @NonNull ScreenLength height,
                         final @ScrollBehaviorType int behavior) {
        final GeckoBundle msg = buildScrollMessage(width, height, behavior);
        mSession.getEventDispatcher().dispatch("GeckoView:ScrollTo", msg);
    }

    /**
     * Scroll to the top left corner of the screen.
     * Uses {@link #SCROLL_BEHAVIOR_SMOOTH}.
     */
    @UiThread
    public void scrollToTop() {
        scrollTo(ScreenLength.zero(), ScreenLength.top(), SCROLL_BEHAVIOR_SMOOTH);
    }

    /**
     * Scroll to the bottom left corner of the screen.
     * Uses {@link #SCROLL_BEHAVIOR_SMOOTH}.
     */
    @UiThread
    public void scrollToBottom() {
        scrollTo(ScreenLength.zero(), ScreenLength.bottom(), SCROLL_BEHAVIOR_SMOOTH);
    }

    private GeckoBundle buildScrollMessage(final @NonNull ScreenLength width,
                                           final @NonNull ScreenLength height,
                                           final @ScrollBehaviorType int behavior) {
        final GeckoBundle msg = new GeckoBundle();
        msg.putDouble("widthValue", width.getValue());
        msg.putInt("widthType", width.getType());
        msg.putDouble("heightValue", height.getValue());
        msg.putInt("heightType", height.getType());
        msg.putInt("behavior", behavior);
        return msg;
    }
}
