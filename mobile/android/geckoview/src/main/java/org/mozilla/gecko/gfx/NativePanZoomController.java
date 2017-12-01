/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.mozglue.JNIObject;
import org.mozilla.gecko.util.ThreadUtils;

import android.graphics.Matrix;
import android.graphics.Rect;
import android.os.SystemClock;
import android.util.Log;
import android.view.MotionEvent;
import android.view.InputDevice;

import java.util.ArrayList;

public class NativePanZoomController extends JNIObject {
    private static final String LOGTAG = "GeckoNPZC";

    private final LayerSession mSession;
    private final Rect mTempRect = new Rect();
    private boolean mAttached;
    private float mPointerScrollFactor = 64.0f;
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
        if (!mAttached) {
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
        if (!mAttached) {
            return false;
        }

        final int count = event.getPointerCount();

        if (count <= 0) {
            return false;
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

        return handleScrollEvent(event.getEventTime(), event.getMetaState(), x, y,
                                 hScroll, vScroll);
    }

    private boolean handleMouseEvent(MotionEvent event) {
        if (!mAttached) {
            return false;
        }

        final int count = event.getPointerCount();

        if (count <= 0) {
            return false;
        }

        final MotionEvent.PointerCoords coords = new MotionEvent.PointerCoords();
        event.getPointerCoords(0, coords);

        // Translate surface origin to client origin for mouse events.
        mSession.getSurfaceBounds(mTempRect);
        final float x = coords.x - mTempRect.left;
        final float y = coords.y - mTempRect.top;

        return handleMouseEvent(event.getActionMasked(), event.getEventTime(),
                                event.getMetaState(), x, y, event.getButtonState());
    }

    /* package */ NativePanZoomController(final LayerSession session) {
        mSession = session;
    }

    /**
     * Set the current scroll factor. The scroll factor is the maximum scroll amount that
     * one scroll event may generate, in device pixels.
     *
     * @return Scroll factor.
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
     * Process a touch event through the pan-zoom controller. Treat any mouse events as
     * "touch" rather than as "mouse". Pointer coordinates should be relative to the
     * display surface.
     *
     * @param event MotionEvent to process.
     * @return True if the event was handled.
     */
    public boolean onTouchEvent(final MotionEvent event) {
        ThreadUtils.assertOnUiThread();
        return handleMotionEvent(event);
    }

    /**
     * Process a touch event through the pan-zoom controller. Treat any mouse events as
     * "mouse" rather than as "touch". Pointer coordinates should be relative to the
     * display surface.
     *
     * @param event MotionEvent to process.
     * @return True if the event was handled.
     */
    public boolean onMouseEvent(final MotionEvent event) {
        ThreadUtils.assertOnUiThread();

        if (event.getToolType(0) == MotionEvent.TOOL_TYPE_MOUSE) {
            return handleMouseEvent(event);
        }
        return handleMotionEvent(event);
    }

    /**
     * Process a non-touch motion event through the pan-zoom controller. Currently, hover
     * and scroll events are supported. Pointer coordinates should be relative to the
     * display surface.
     *
     * @param event MotionEvent to process.
     * @return True if the event was handled.
     */
    public boolean onMotionEvent(MotionEvent event) {
        ThreadUtils.assertOnUiThread();

        final int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_SCROLL) {
            if (event.getDownTime() >= mLastDownTime) {
                mLastDownTime = event.getDownTime();
            } else if ((InputDevice.getDevice(event.getDeviceId()).getSources() &
                        InputDevice.SOURCE_TOUCHPAD) == InputDevice.SOURCE_TOUCHPAD) {
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

    @WrapForJNI(calledFrom = "ui")
    private void setAttached(final boolean attached) {
        if (attached) {
            mAttached = true;
        } else if (mAttached) {
            mAttached = false;
            disposeNative();
        }
    }

    @WrapForJNI(calledFrom = "ui", dispatchTo = "gecko") @Override // JNIObject
    protected native void disposeNative();

    @WrapForJNI(stubName = "SetIsLongpressEnabled") // Called from test thread.
    private native void nativeSetIsLongpressEnabled(boolean isLongpressEnabled);

    /**
     * Set whether Gecko should generate long-press events.
     *
     * @param isLongpressEnabled True if Gecko should generate long-press events.
     */
    public void setIsLongpressEnabled(boolean isLongpressEnabled) {
        ThreadUtils.assertOnUiThread();

        if (mAttached) {
            nativeSetIsLongpressEnabled(isLongpressEnabled);
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

        public MotionEvent.PointerCoords getCoords() {
            MotionEvent.PointerCoords coords = new MotionEvent.PointerCoords();
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
                                         int clientX, int clientY, double pressure,
                                         int orientation)
    {
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

        // Translate client origin to surface origin.
        mSession.getSurfaceBounds(mTempRect);
        final int surfaceX = clientX + mTempRect.left;
        final int surfaceY = clientY + mTempRect.top;

        // Update the pointer with the new info
        PointerInfo info = mPointerState.pointers.get(pointerIndex);
        info.surfaceX = surfaceX;
        info.surfaceY = surfaceY;
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
        onTouchEvent(event);

        // Forget about removed pointers
        if (eventType == MotionEvent.ACTION_POINTER_UP ||
            eventType == MotionEvent.ACTION_UP ||
            eventType == MotionEvent.ACTION_CANCEL ||
            eventType == MotionEvent.ACTION_HOVER_MOVE)
        {
            mPointerState.pointers.remove(pointerIndex);
        }
    }

    @WrapForJNI(calledFrom = "ui")
    private void synthesizeNativeTouchPoint(int pointerId, int eventType, int clientX,
                                            int clientY, double pressure, int orientation) {
        if (pointerId == PointerInfo.RESERVED_MOUSE_POINTER_ID) {
            throw new IllegalArgumentException("Pointer ID reserved for mouse");
        }
        synthesizeNativePointer(InputDevice.SOURCE_TOUCHSCREEN, pointerId,
                                eventType, clientX, clientY, pressure, orientation);
    }

    @WrapForJNI(calledFrom = "ui")
    private void synthesizeNativeMouseEvent(int eventType, int clientX, int clientY) {
        synthesizeNativePointer(InputDevice.SOURCE_MOUSE,
                                PointerInfo.RESERVED_MOUSE_POINTER_ID,
                                eventType, clientX, clientY, 0, 0);
    }
}
