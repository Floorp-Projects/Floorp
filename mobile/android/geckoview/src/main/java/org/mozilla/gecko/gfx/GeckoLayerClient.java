/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.util.GeckoBundle;

import android.content.Context;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.PointF;
import android.graphics.RectF;
import android.os.SystemClock;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.InputDevice;
import android.view.MotionEvent;

import java.util.ArrayList;

class GeckoLayerClient implements LayerView.Listener
{
    private static final String LOGTAG = "GeckoLayerClient";

    private final Context mContext;
    private IntSize mScreenSize;
    private IntSize mWindowSize;

    private boolean mForceRedraw;

    /* The current viewport metrics.
     * This is volatile so that we can read and write to it from different threads.
     * We avoid synchronization to make getting the viewport metrics from
     * the compositor as cheap as possible. The viewport is immutable so
     * we don't need to worry about anyone mutating it while we're reading from it.
     * Specifically:
     * 1) reading mViewportMetrics from any thread is fine without synchronization
     * 2) writing to mViewportMetrics requires synchronizing on the layer controller object
     * 3) whenever reading multiple fields from mViewportMetrics without synchronization (i.e. in
     *    case 1 above) you should always first grab a local copy of the reference, and then use
     *    that because mViewportMetrics might get reassigned in between reading the different
     *    fields. */
    private volatile ImmutableViewportMetrics mViewportMetrics;

    private volatile boolean mGeckoIsReady;

    /* package */ final PanZoomController mPanZoomController;
    private final DynamicToolbarAnimator mToolbarAnimator;
    /* package */ final LayerView mView;

    /* This flag is true from the time that browser.js detects a first-paint is about to start,
     * to the time that we receive the first-paint composite notification from the compositor.
     * Note that there is a small race condition with this; if there are two paints that both
     * have the first-paint flag set, and the second paint happens concurrently with the
     * composite for the first paint, then this flag may be set to true prematurely. Fixing this
     * is possible but risky; see https://bugzilla.mozilla.org/show_bug.cgi?id=797615#c751
     */
    private volatile boolean mContentDocumentIsDisplayed;

    private SynthesizedEventState mPointerState;

    public GeckoLayerClient(Context context, LayerView view) {
        // we can fill these in with dummy values because they are always written
        // to before being read
        mContext = context;
        mScreenSize = new IntSize(0, 0);
        mWindowSize = new IntSize(0, 0);

        mForceRedraw = true;
        DisplayMetrics displayMetrics = context.getResources().getDisplayMetrics();
        mViewportMetrics = new ImmutableViewportMetrics(displayMetrics)
                           .setViewportSize(view.getWidth(), view.getHeight());

        mToolbarAnimator = new DynamicToolbarAnimator(this);
        mPanZoomController = PanZoomController.Factory.create(view);
        mView = view;
        mView.setListener(this);
        mContentDocumentIsDisplayed = true;
    }

    public void setOverscrollHandler(final Overscroll listener) {
        mPanZoomController.setOverscrollHandler(listener);
    }

    public void setGeckoReady(boolean ready) {
        mGeckoIsReady = ready;
    }

    public boolean isGeckoReady() {
        return mGeckoIsReady;
    }

    /** Attaches to root layer so that Gecko appears. */
    @WrapForJNI(calledFrom = "gecko")
    private void onGeckoReady() {
        mGeckoIsReady = true;

        sendResizeEventIfNecessary(true);

        // Gecko being ready is one of the two conditions (along with having an available
        // surface) that cause us to create the compositor. So here, now that we know gecko
        // is ready, call updateCompositor() to see if we can actually do the creation.
        // This needs to run on the UI thread so that the surface validity can't change on
        // us while we're in the middle of creating the compositor.
        mView.post(new Runnable() {
            @Override
            public void run() {
                mPanZoomController.attach();
                mView.updateCompositor();
            }
        });
    }

    public void destroy() {
        mPanZoomController.destroy();
        mGeckoIsReady = false;
    }

    public LayerView getView() {
        return mView;
    }

    public FloatSize getViewportSize() {
        return mViewportMetrics.getSize();
    }

    /**
     * The view calls this function to indicate that the viewport changed size. It must hold the
     * monitor while calling it.
     *
     * TODO: Refactor this to use an interface. Expose that interface only to the view and not
     * to the layer client. That way, the layer client won't be tempted to call this, which might
     * result in an infinite loop.
     */
    boolean setViewportSize(int width, int height) {
        if (mViewportMetrics.viewportRectWidth == width &&
            mViewportMetrics.viewportRectHeight == height) {
            return false;
        }
        mViewportMetrics = mViewportMetrics.setViewportSize(width, height);

        if (mGeckoIsReady) {
            // here we send gecko a resize message. The code in browser.js is responsible for
            // picking up on that resize event, modifying the viewport as necessary, and informing
            // us of the new viewport.
            sendResizeEventIfNecessary(true);

            // the following call also sends gecko a message, which will be processed after the resize
            // message above has updated the viewport. this message ensures that if we have just put
            // focus in a text field, we scroll the content so that the text field is in view.
            GeckoAppShell.viewSizeChanged();
        }
        return true;
    }

    PanZoomController getPanZoomController() {
        return mPanZoomController;
    }

    DynamicToolbarAnimator getDynamicToolbarAnimator() {
        return mToolbarAnimator;
    }

    /* Informs Gecko that the screen size has changed. */
    private void sendResizeEventIfNecessary(boolean force) {
        DisplayMetrics metrics = mContext.getResources().getDisplayMetrics();

        IntSize newScreenSize = new IntSize(metrics.widthPixels, metrics.heightPixels);
        IntSize newWindowSize = new IntSize(mViewportMetrics.viewportRectWidth,
                                            mViewportMetrics.viewportRectHeight);

        boolean screenSizeChanged = !mScreenSize.equals(newScreenSize);
        boolean windowSizeChanged = !mWindowSize.equals(newWindowSize);

        if (!force && !screenSizeChanged && !windowSizeChanged) {
            return;
        }

        mScreenSize = newScreenSize;
        mWindowSize = newWindowSize;

        if (screenSizeChanged) {
            Log.d(LOGTAG, "Screen-size changed to " + mScreenSize);
        }

        if (windowSizeChanged) {
            Log.d(LOGTAG, "Window-size changed to " + mWindowSize);
        }

        if (mView != null) {
            mView.notifySizeChanged(mWindowSize.width, mWindowSize.height,
                                    mScreenSize.width, mScreenSize.height);
        }
    }

    @WrapForJNI(calledFrom = "gecko")
    void contentDocumentChanged() {
        mContentDocumentIsDisplayed = false;
    }

    @WrapForJNI(calledFrom = "gecko")
    boolean isContentDocumentDisplayed() {
        return mContentDocumentIsDisplayed;
    }

    /** The compositor invokes this function just before compositing a frame where the document
      * is different from the document composited on the last frame. In these cases, the viewport
      * information we have in Java is no longer valid and needs to be replaced with the new
      * viewport information provided.
      */
    @WrapForJNI(calledFrom = "ui")
    public void updateRootFrameMetrics(float scrollX, float scrollY, float zoom) {
        mViewportMetrics = mViewportMetrics.setViewportOrigin(scrollX, scrollY)
            .setZoomFactor(zoom);

        mToolbarAnimator.onMetricsChanged(mViewportMetrics);
        mContentDocumentIsDisplayed = true;
    }

    class PointerInfo {
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

    class SynthesizedEventState {
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
            MotionEvent.PointerProperties[] props = new MotionEvent.PointerProperties[getPointerCount(source)];
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
            MotionEvent.PointerCoords[] coords = new MotionEvent.PointerCoords[getPointerCount(source)];
            int index = 0;
            for (int i = 0; i < pointers.size(); i++) {
                if (pointers.get(i).source == source) {
                    coords[index++] = pointers.get(i).getCoords();
                }
            }
            return coords;
        }
    }

    private void synthesizeNativePointer(int source, int pointerId,
            int eventType, int screenX, int screenY, double pressure,
            int orientation)
    {
        Log.d(LOGTAG, "Synthesizing pointer from " + source + " id " + pointerId + " at " + screenX + ", " + screenY);

        if (mPointerState == null) {
            mPointerState = new SynthesizedEventState();
        }

        // Find the pointer if it already exists
        int pointerIndex = mPointerState.getPointerIndex(pointerId);

        // Event-specific handling
        switch (eventType) {
            case MotionEvent.ACTION_POINTER_UP:
                if (pointerIndex < 0) {
                    Log.d(LOGTAG, "Requested synthesis of a pointer-up for a pointer that doesn't exist!");
                    return;
                }
                if (mPointerState.pointers.size() == 1) {
                    // Last pointer is going up
                    eventType = MotionEvent.ACTION_UP;
                }
                break;
            case MotionEvent.ACTION_CANCEL:
                if (pointerIndex < 0) {
                    Log.d(LOGTAG, "Requested synthesis of a pointer-cancel for a pointer that doesn't exist!");
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
        if (mView != null) {
            info.screenY += mView.getCurrentToolbarHeight();
        }
        info.pressure = pressure;
        info.orientation = orientation;

        // Dispatch the event
        int action = (pointerIndex << MotionEvent.ACTION_POINTER_INDEX_SHIFT);
        action &= MotionEvent.ACTION_POINTER_INDEX_MASK;
        action |= (eventType & MotionEvent.ACTION_MASK);
        boolean isButtonDown = (source == InputDevice.SOURCE_MOUSE) &&
                               (eventType == MotionEvent.ACTION_DOWN || eventType == MotionEvent.ACTION_MOVE);
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
        mView.post(new Runnable() {
            @Override
            public void run() {
                mView.dispatchTouchEvent(event);
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
    public void synthesizeNativeTouchPoint(int pointerId, int eventType, int screenX,
            int screenY, double pressure, int orientation)
    {
        if (pointerId == PointerInfo.RESERVED_MOUSE_POINTER_ID) {
            throw new IllegalArgumentException("Use a different pointer ID in your test, this one is reserved for mouse");
        }
        synthesizeNativePointer(InputDevice.SOURCE_TOUCHSCREEN, pointerId,
            eventType, screenX, screenY, pressure, orientation);
    }

    @WrapForJNI(calledFrom = "gecko")
    public void synthesizeNativeMouseEvent(int eventType, int screenX, int screenY) {
        synthesizeNativePointer(InputDevice.SOURCE_MOUSE, PointerInfo.RESERVED_MOUSE_POINTER_ID,
            eventType, screenX, screenY, 0, 0);
    }

    /** Implementation of LayerView.Listener */
    @Override
    public void surfaceChanged() {
        IntSize viewportSize = mToolbarAnimator.getViewportSize();
        setViewportSize(viewportSize.width, viewportSize.height);
    }

    ImmutableViewportMetrics getViewportMetrics() {
        return mViewportMetrics;
    }

    Matrix getMatrixForLayerRectToViewRect() {
        if (!mGeckoIsReady) {
            return null;
        }

        ImmutableViewportMetrics viewportMetrics = mViewportMetrics;
        PointF origin = viewportMetrics.getOrigin();
        float zoom = viewportMetrics.zoomFactor;
        ImmutableViewportMetrics geckoViewport = mViewportMetrics;
        PointF geckoOrigin = geckoViewport.getOrigin();
        float geckoZoom = geckoViewport.zoomFactor;

        Matrix matrix = new Matrix();
        matrix.postTranslate(geckoOrigin.x / geckoZoom, geckoOrigin.y / geckoZoom);
        matrix.postScale(zoom, zoom);
        matrix.postTranslate(-origin.x, -origin.y);
        return matrix;
    }
}
