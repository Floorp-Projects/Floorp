/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.PrefsHelper;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.gfx.DynamicToolbarAnimator.PinReason;
import org.mozilla.gecko.mozglue.JNIObject;
import org.mozilla.gecko.util.ThreadUtils;

import org.json.JSONObject;

import android.graphics.PointF;
import android.util.TypedValue;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.InputDevice;

class NativePanZoomController extends JNIObject implements PanZoomController {
    private final LayerView mView;
    private boolean mDestroyed;
    private Overscroll mOverscroll;
    boolean mNegateWheelScroll;
    private float mPointerScrollFactor;
    private PrefsHelper.PrefHandler mPrefsObserver;
    private long mLastDownTime;
    private static final float MAX_SCROLL = 0.075f * GeckoAppShell.getDpi();

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
        final float y = coords.y - mView.getCurrentToolbarHeight();

        final float flipFactor = mNegateWheelScroll ? -1.0f : 1.0f;
        final float hScroll = event.getAxisValue(MotionEvent.AXIS_HSCROLL) * flipFactor * mPointerScrollFactor;
        final float vScroll = event.getAxisValue(MotionEvent.AXIS_VSCROLL) * flipFactor * mPointerScrollFactor;

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
        final float y = coords.y;

        return handleMouseEvent(event.getActionMasked(), event.getEventTime(), event.getMetaState(), x, y, event.getButtonState());
    }


    NativePanZoomController(View view) {
        mView = (LayerView) view;
        mDestroyed = true;

        String[] prefs = { "ui.scrolling.negate_wheel_scroll" };
        mPrefsObserver = new PrefsHelper.PrefHandlerBase() {
            @Override public void prefValue(String pref, boolean value) {
                if (pref.equals("ui.scrolling.negate_wheel_scroll")) {
                    mNegateWheelScroll = value;
                }
            }
        };
        PrefsHelper.addObserver(prefs, mPrefsObserver);

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
        if (mPrefsObserver != null) {
            PrefsHelper.removeObserver(mPrefsObserver);
            mPrefsObserver = null;
        }
        if (mDestroyed || !mView.isGeckoReady()) {
            return;
        }
        mDestroyed = true;
        disposeNative();
    }

    @Override
    public void attach() {
        mDestroyed = false;
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
    private void onSelectionDragState(boolean state) {
        mView.getDynamicToolbarAnimator().setPinned(state, PinReason.CARET_DRAG);
    }
}
