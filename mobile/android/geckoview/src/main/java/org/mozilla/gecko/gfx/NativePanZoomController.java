/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.gfx.DynamicToolbarAnimator.PinReason;
import org.mozilla.gecko.mozglue.JNIObject;
import org.mozilla.gecko.util.ThreadUtils;

import org.json.JSONObject;

import android.graphics.PointF;
import android.os.SystemClock;
import android.util.Log;
import android.util.TypedValue;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.InputDevice;

import java.util.ArrayList;

class NativePanZoomController extends JNIObject implements PanZoomController {
    private static final String LOGTAG = "GeckoNPZC";
    private final float MAX_SCROLL;

    private final LayerView mView;

    private boolean mDestroyed;
    private Overscroll mOverscroll;
    private float mPointerScrollFactor;
    private long mLastDownTime;

    private SynthesizedEventState mPointerState;

    @WrapForJNI(calledFrom = "ui")
    private native boolean handleMotionEvent(
            int action, int actionIndex, long time, int metaState,
            int pointerId[], float x[], float y[], float orientation[], float pressure[],
            float toolMajor[], float toolMinor[]);

    @WrapForJNI(calledFrom = "ui")
    private native boolean handleScrollEvent(
            long time, int metaState,
            float x, float y,
            float hScroll, float vScroll);

    @WrapForJNI(calledFrom = "ui")
    private native boolean handleMouseEvent(
            int action, long time, int metaState,
            float x, float y, int buttons);

    private boolean handleMotionEvent(MotionEvent event) {
        if (mDestroyed) {
            return false;
        }

        final int action = event.getActionMasked();
        final int count = event.getPointerCount();

        if (action == MotionEvent.ACTION_DOWN) {
            mLastDownTime = event.getDownTime();
        } else if (mLastDownTime != event.getDownTime()) {
            return false;
        }

        final int[] pointerId = new int[count];
        final float[] x = new float[count];
        final float[] y = new float[count];
        final float[] orientation = new float[count];
        final float[] pressure = new float[count];
        final float[] toolMajor = new float[count];
        final float[] toolMinor = new float[count];

        final MotionEvent.PointerCoords coords = new MotionEvent.PointerCoords();

        for (int i = 0; i < count; i++) {
            pointerId[i] = event.getPointerId(i);
            event.getPointerCoords(i, coords);

            x[i] = coords.x;
            y[i] = coords.y;

            orientation[i] = coords.orientation;
            pressure[i] = coords.pressure;

            // If we are converting to CSS pixels, we should adjust the radii as well.
            toolMajor[i] = coords.toolMajor;
            toolMinor[i] = coords.toolMinor;
        }

        return handleMotionEvent(action, event.getActionIndex(), event.getEventTime(),
                event.getMetaState(), pointerId, x, y, orientation, pressure,
                toolMajor, toolMinor);
    }

    private boolean handleScrollEvent(MotionEvent event) {
        if (mDestroyed) {
            return false;
        }

        final int count = event.getPointerCount();

        if (count <= 0) {
            return false;
        }

        final MotionEvent.PointerCoords coords = new MotionEvent.PointerCoords();
        event.getPointerCoords(0, coords);
        final float x = coords.x;
        // Scroll events are not adjusted by the AndroidDyanmicToolbarAnimator so adjust the offset here.
        final int toolbarHeight = (mView.mSession != null) ?
            mView.mSession.getDynamicToolbarAnimator().getCurrentToolbarHeight() : 0;
        final float y = coords.y - toolbarHeight;

        final float hScroll = event.getAxisValue(MotionEvent.AXIS_HSCROLL) *
                              mPointerScrollFactor;
        final float vScroll = event.getAxisValue(MotionEvent.AXIS_VSCROLL) *
                              mPointerScrollFactor;

        return handleScrollEvent(event.getEventTime(), event.getMetaState(), x, y, hScroll, vScroll);
    }

    private boolean handleMouseEvent(MotionEvent event) {
        if (mDestroyed) {
            return false;
        }

        final int count = event.getPointerCount();

        if (count <= 0) {
            return false;
        }

        final MotionEvent.PointerCoords coords = new MotionEvent.PointerCoords();
        event.getPointerCoords(0, coords);
        final float x = coords.x;
        // Mouse events are not adjusted by the AndroidDyanmicToolbarAnimator so adjust the offset
        // here.
        final int toolbarHeight = (mView.mSession != null) ?
            mView.mSession.getDynamicToolbarAnimator().getCurrentToolbarHeight() : 0;
        final float y = coords.y - toolbarHeight;

        return handleMouseEvent(event.getActionMasked(), event.getEventTime(), event.getMetaState(), x, y, event.getButtonState());
    }


    NativePanZoomController(View view) {
        MAX_SCROLL = 0.075f * view.getContext().getResources().getDisplayMetrics().densityDpi;

        mView = (LayerView) view;

        TypedValue outValue = new TypedValue();
        if (view.getContext().getTheme().resolveAttribute(android.R.attr.listPreferredItemHeight, outValue, true)) {
            mPointerScrollFactor = outValue.getDimension(view.getContext().getResources().getDisplayMetrics());
        } else {
            mPointerScrollFactor = MAX_SCROLL;
        }
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
// NOTE: This commented out block of code allows Fennec to generate
//       mouse event instead of converting them to touch events.
//       This gives Fennec similar behaviour to desktop when using
//       a mouse.
//
//        if (event.getToolType(0) == MotionEvent.TOOL_TYPE_MOUSE) {
//            return handleMouseEvent(event);
//        } else {
//            return handleMotionEvent(event);
//        }
        return handleMotionEvent(event);
    }

    @Override
    public boolean onMotionEvent(MotionEvent event) {
        final int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_SCROLL) {
            if (event.getDownTime() >= mLastDownTime) {
                mLastDownTime = event.getDownTime();
            } else if ((InputDevice.getDevice(event.getDeviceId()).getSources() & InputDevice.SOURCE_TOUCHPAD) == InputDevice.SOURCE_TOUCHPAD) {
                return false;
            }
            return handleScrollEvent(event);
        } else if ((action == MotionEvent.ACTION_HOVER_MOVE) ||
                   (action == MotionEvent.ACTION_HOVER_ENTER) ||
                   (action == MotionEvent.ACTION_HOVER_EXIT)) {
            return handleMouseEvent(event);
        } else {
            return false;
        }
    }

    @Override @WrapForJNI(calledFrom = "ui") // PanZoomController
    public void destroy() {
        if (mDestroyed || !mView.isCompositorReady()) {
            return;
        }
        mDestroyed = true;
        disposeNative();
    }

    @WrapForJNI(calledFrom = "ui", dispatchTo = "gecko_priority") @Override // JNIObject
    protected native void disposeNative();

    @Override
    public void setOverscrollHandler(final Overscroll handler) {
        mOverscroll = handler;
    }

    @WrapForJNI(stubName = "SetIsLongpressEnabled") // Called from test thread.
    private native void nativeSetIsLongpressEnabled(boolean isLongpressEnabled);

    @Override // PanZoomController
    public void setIsLongpressEnabled(boolean isLongpressEnabled) {
        if (!mDestroyed) {
            nativeSetIsLongpressEnabled(isLongpressEnabled);
        }
    }

    @WrapForJNI
    private void updateOverscrollVelocity(final float x, final float y) {
        if (mOverscroll != null) {
            if (ThreadUtils.isOnUiThread() == true) {
                mOverscroll.setVelocity(x * 1000.0f, Overscroll.Axis.X);
                mOverscroll.setVelocity(y * 1000.0f, Overscroll.Axis.Y);
            } else {
                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        // Multiply the velocity by 1000 to match what was done in JPZ.
                        mOverscroll.setVelocity(x * 1000.0f, Overscroll.Axis.X);
                        mOverscroll.setVelocity(y * 1000.0f, Overscroll.Axis.Y);
                    }
                });
            }
        }
    }

    @WrapForJNI
    private void updateOverscrollOffset(final float x, final float y) {
        if (mOverscroll != null) {
            if (ThreadUtils.isOnUiThread() == true) {
                mOverscroll.setDistance(x, Overscroll.Axis.X);
                mOverscroll.setDistance(y, Overscroll.Axis.Y);
            } else {
                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        mOverscroll.setDistance(x, Overscroll.Axis.X);
                        mOverscroll.setDistance(y, Overscroll.Axis.Y);
                    }
                });
            }
        }
    }

    /**
     * Active SelectionCaretDrag requires DynamicToolbarAnimator to be pinned
     * to avoid unwanted scroll interactions.
     */
    @WrapForJNI(calledFrom = "gecko")
    private void onSelectionDragState(final boolean state) {
        final LayerView view = mView;
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                if (view.mSession != null) {
                    view.mSession.getDynamicToolbarAnimator()
                                 .setPinned(state, PinReason.CARET_DRAG);
                }
            }
        });
    }

    private static class PointerInfo {
        // We reserve one pointer ID for the mouse, so that tests don't have
        // to worry about tracking pointer IDs if they just want to test mouse
        // event synthesization. If somebody tries to use this ID for a
        // synthesized touch event we'll throw an exception.
        public static final int RESERVED_MOUSE_POINTER_ID = 100000;

        public int pointerId;
        public int source;
        public int screenX;
        public int screenY;
        public double pressure;
        public int orientation;

        public MotionEvent.PointerCoords getCoords() {
            MotionEvent.PointerCoords coords = new MotionEvent.PointerCoords();
            coords.orientation = orientation;
            coords.pressure = (float)pressure;
            coords.x = screenX;
            coords.y = screenY;
            return coords;
        }
    }

    private static class SynthesizedEventState {
        public final ArrayList<PointerInfo> pointers;
        public long downTime;

        SynthesizedEventState() {
            pointers = new ArrayList<PointerInfo>();
        }

        int getPointerIndex(int pointerId) {
            for (int i = 0; i < pointers.size(); i++) {
                if (pointers.get(i).pointerId == pointerId) {
                    return i;
                }
            }
            return -1;
        }

        int addPointer(int pointerId, int source) {
            PointerInfo info = new PointerInfo();
            info.pointerId = pointerId;
            info.source = source;
            pointers.add(info);
            return pointers.size() - 1;
        }

        int getPointerCount(int source) {
            int count = 0;
            for (int i = 0; i < pointers.size(); i++) {
                if (pointers.get(i).source == source) {
                    count++;
                }
            }
            return count;
        }

        MotionEvent.PointerProperties[] getPointerProperties(int source) {
            MotionEvent.PointerProperties[] props =
                    new MotionEvent.PointerProperties[getPointerCount(source)];
            int index = 0;
            for (int i = 0; i < pointers.size(); i++) {
                if (pointers.get(i).source == source) {
                    MotionEvent.PointerProperties p = new MotionEvent.PointerProperties();
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

        MotionEvent.PointerCoords[] getPointerCoords(int source) {
            MotionEvent.PointerCoords[] coords =
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

    private void synthesizeNativePointer(int source, int pointerId, int eventType,
                                         int screenX, int screenY, double pressure,
                                         int orientation)
    {
        final View view = mView;
        final int[] origin = new int[2];
        view.getLocationOnScreen(origin);
        screenX -= origin[0];
        screenY -= origin[1];

        if (mPointerState == null) {
            mPointerState = new SynthesizedEventState();
        }

        // Find the pointer if it already exists
        int pointerIndex = mPointerState.getPointerIndex(pointerId);

        // Event-specific handling
        switch (eventType) {
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

        // Update the pointer with the new info
        PointerInfo info = mPointerState.pointers.get(pointerIndex);
        info.screenX = screenX;
        info.screenY = screenY;
        info.pressure = pressure;
        info.orientation = orientation;

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
        boolean isButtonDown = (source == InputDevice.SOURCE_MOUSE) &&
                               (eventType == MotionEvent.ACTION_DOWN ||
                                eventType == MotionEvent.ACTION_MOVE);
        final MotionEvent event = MotionEvent.obtain(
            /*downTime*/ mPointerState.downTime,
            /*eventTime*/ SystemClock.uptimeMillis(),
            /*action*/ action,
            /*pointerCount*/ mPointerState.getPointerCount(source),
            /*pointerProperties*/ mPointerState.getPointerProperties(source),
            /*pointerCoords*/ mPointerState.getPointerCoords(source),
            /*metaState*/ 0,
            /*buttonState*/ (isButtonDown ? MotionEvent.BUTTON_PRIMARY : 0),
            /*xPrecision*/ 0,
            /*yPrecision*/ 0,
            /*deviceId*/ 0,
            /*edgeFlags*/ 0,
            /*source*/ source,
            /*flags*/ 0);
        view.post(new Runnable() {
            @Override
            public void run() {
                view.dispatchTouchEvent(event);
            }
        });

        // Forget about removed pointers
        if (eventType == MotionEvent.ACTION_POINTER_UP ||
            eventType == MotionEvent.ACTION_UP ||
            eventType == MotionEvent.ACTION_CANCEL ||
            eventType == MotionEvent.ACTION_HOVER_MOVE)
        {
            mPointerState.pointers.remove(pointerIndex);
        }
    }

    @WrapForJNI(calledFrom = "gecko")
    private void synthesizeNativeTouchPoint(int pointerId, int eventType, int screenX,
            int screenY, double pressure, int orientation)
    {
        if (pointerId == PointerInfo.RESERVED_MOUSE_POINTER_ID) {
            throw new IllegalArgumentException("Pointer ID reserved for mouse");
        }
        synthesizeNativePointer(InputDevice.SOURCE_TOUCHSCREEN, pointerId,
                                eventType, screenX, screenY, pressure, orientation);
    }

    @WrapForJNI(calledFrom = "gecko")
    private void synthesizeNativeMouseEvent(int eventType, int screenX, int screenY) {
        synthesizeNativePointer(InputDevice.SOURCE_MOUSE,
                                PointerInfo.RESERVED_MOUSE_POINTER_ID,
                                eventType, screenX, screenY, 0, 0);
    }
}
