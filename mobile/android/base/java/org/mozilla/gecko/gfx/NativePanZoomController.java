/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.mozglue.JNIObject;
import org.mozilla.gecko.util.ThreadUtils;

import org.json.JSONObject;

import android.graphics.PointF;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;

class NativePanZoomController extends JNIObject implements PanZoomController {
    private final PanZoomTarget mTarget;
    private final LayerView mView;
    private boolean mDestroyed;
    private Overscroll mOverscroll;

    @WrapForJNI
    private native boolean handleMotionEvent(
            int action, int actionIndex, long time, int metaState,
            int pointerId[], float x[], float y[], float orientation[], float pressure[],
            float toolMajor[], float toolMinor[]);

    private boolean handleMotionEvent(MotionEvent event, boolean keepInViewCoordinates) {
        if (mDestroyed) {
            return false;
        }

        final int action = event.getActionMasked();
        final int count = event.getPointerCount();

        final int[] pointerId = new int[count];
        final float[] x = new float[count];
        final float[] y = new float[count];
        final float[] orientation = new float[count];
        final float[] pressure = new float[count];
        final float[] toolMajor = new float[count];
        final float[] toolMinor = new float[count];

        final MotionEvent.PointerCoords coords = new MotionEvent.PointerCoords();
        final PointF point = !keepInViewCoordinates ? new PointF() : null;
        final float zoom = !keepInViewCoordinates ? mView.getViewportMetrics().zoomFactor : 1.0f;

        for (int i = 0; i < count; i++) {
            pointerId[i] = event.getPointerId(i);
            event.getPointerCoords(i, coords);

            if (keepInViewCoordinates) {
                x[i] = coords.x;
                y[i] = coords.y;
            } else {
                point.x = coords.x;
                point.y = coords.y;
                final PointF newPoint = mView.convertViewPointToLayerPoint(point);
                x[i] = newPoint.x;
                y[i] = newPoint.y;
            }

            orientation[i] = coords.orientation;
            pressure[i] = coords.pressure;

            // If we are converting to CSS pixels, we should adjust the radii as well.
            toolMajor[i] = coords.toolMajor / zoom;
            toolMinor[i] = coords.toolMinor / zoom;
        }

        return handleMotionEvent(action, event.getActionIndex(), event.getEventTime(),
                event.getMetaState(), pointerId, x, y, orientation, pressure,
                toolMajor, toolMinor);
    }

    NativePanZoomController(PanZoomTarget target, View view) {
        mTarget = target;
        mView = (LayerView) view;
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        return handleMotionEvent(event, /* keepInViewCoordinates */ true);
    }

    @Override
    public boolean onMotionEvent(MotionEvent event) {
        // FIXME implement this
        return false;
    }

    @Override
    public boolean onKeyEvent(KeyEvent event) {
        // FIXME implement this
        return false;
    }

    @Override
    public PointF getVelocityVector() {
        // FIXME implement this
        return new PointF(0, 0);
    }

    @Override
    public void pageRectUpdated() {
        // no-op in APZC, I think
    }

    @Override
    public void abortPanning() {
        // no-op in APZC, I think
    }

    @Override
    public void notifyDefaultActionPrevented(boolean prevented) {
        // no-op: This could get called if accessibility is enabled and the events
        // are sent to Gecko directly without going through APZ. In this case
        // we just want to ignore this callback.
    }

    @WrapForJNI(stubName = "AbortAnimation")
    private native void nativeAbortAnimation();

    @Override // PanZoomController
    public void abortAnimation()
    {
        if (!mDestroyed) {
            nativeAbortAnimation();
        }
    }

    @Override // PanZoomController
    public boolean getRedrawHint()
    {
        // FIXME implement this
        return true;
    }

    @Override @WrapForJNI(allowMultithread = true) // PanZoomController
    public void destroy() {
        if (mDestroyed) {
            return;
        }
        mDestroyed = true;
        disposeNative();
    }

    @Override @WrapForJNI // JNIObject
    protected native void disposeNative();

    @Override
    public void setOverScrollMode(int overscrollMode) {
        // FIXME implement this
    }

    @Override
    public int getOverScrollMode() {
        // FIXME implement this
        return 0;
    }

    @WrapForJNI(allowMultithread = true, stubName = "RequestContentRepaintWrapper")
    private void requestContentRepaint(float x, float y, float width, float height, float resolution) {
        mTarget.forceRedraw(new DisplayPortMetrics(x, y, x + width, y + height, resolution));
    }

    @Override
    public void setOverscrollHandler(final Overscroll handler) {
        mOverscroll = handler;
    }

    @WrapForJNI(stubName = "SetIsLongpressEnabled")
    private native void nativeSetIsLongpressEnabled(boolean isLongpressEnabled);

    @Override // PanZoomController
    public void setIsLongpressEnabled(boolean isLongpressEnabled) {
        if (!mDestroyed) {
            nativeSetIsLongpressEnabled(isLongpressEnabled);
        }
    }

    @WrapForJNI(allowMultithread = true)
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

    @WrapForJNI(allowMultithread = true)
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
}
