/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.gfx.DynamicToolbarAnimator;
import org.mozilla.gecko.gfx.NativePanZoomController;
import org.mozilla.gecko.gfx.GeckoDisplay;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.Region;
import android.os.Build;
import android.os.Handler;
import android.os.Parcel;
import android.os.Parcelable;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.util.TypedValue;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityManager;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;
import android.widget.FrameLayout;

public class GeckoView extends FrameLayout {
    private static final String LOGTAG = "GeckoView";
    private static final boolean DEBUG = false;

    private static AccessibilityManager sAccessibilityManager;

    protected final Display mDisplay = new Display();
    protected GeckoSession mSession;
    private boolean mStateSaved;

    protected SurfaceView mSurfaceView;

    private InputConnectionListener mInputConnectionListener;
    private boolean mIsResettingFocus;

    private static class SavedState extends BaseSavedState {
        public final GeckoSession session;

        public SavedState(final Parcelable superState, final GeckoSession session) {
            super(superState);
            this.session = session;
        }

        /* package */ SavedState(final Parcel in) {
            super(in);
            session = in.readParcelable(getClass().getClassLoader());
        }

        @Override // BaseSavedState
        public void writeToParcel(final Parcel dest, final int flags) {
            super.writeToParcel(dest, flags);
            dest.writeParcelable(session, flags);
        }

        public static final Creator<SavedState> CREATOR = new Creator<SavedState>() {
            @Override
            public SavedState createFromParcel(final Parcel in) {
                return new SavedState(in);
            }

            @Override
            public SavedState[] newArray(final int size) {
                return new SavedState[size];
            }
        };
    }

    private class Display implements GeckoDisplay,
                                     SurfaceHolder.Callback {
        private final int[] mOrigin = new int[2];

        private Listener mListener;
        private boolean mValid;

        @Override // GeckoDisplay
        public Listener getListener() {
            return mListener;
        }

        @Override // GeckoDisplay
        public void setListener(final Listener listener) {
            if (mValid && mListener != null) {
                // Tell old listener the surface is gone.
                mListener.surfaceDestroyed();
            }

            mListener = listener;

            if (!mValid || listener == null) {
                return;
            }

            // Tell new listener there is already a surface.
            onGlobalLayout();
            if (GeckoView.this.mSurfaceView != null) {
                final SurfaceHolder holder = GeckoView.this.mSurfaceView.getHolder();
                final Rect frame = holder.getSurfaceFrame();
                listener.surfaceChanged(holder.getSurface(), frame.right, frame.bottom);
            }
        }

        @Override // SurfaceHolder.Callback
        public void surfaceCreated(final SurfaceHolder holder) {
        }

        @Override // SurfaceHolder.Callback
        public void surfaceChanged(final SurfaceHolder holder, final int format,
                                   final int width, final int height) {
            if (mListener != null) {
                mListener.surfaceChanged(holder.getSurface(), width, height);
            }
            mValid = true;
        }

        @Override // SurfaceHolder.Callback
        public void surfaceDestroyed(final SurfaceHolder holder) {
            if (mListener != null) {
                mListener.surfaceDestroyed();
            }
            mValid = false;
        }

        public void onGlobalLayout() {
            if (mListener == null) {
                return;
            }
            if (GeckoView.this.mSurfaceView != null) {
                GeckoView.this.mSurfaceView.getLocationOnScreen(mOrigin);
                mListener.screenOriginChanged(mOrigin[0], mOrigin[1]);
            }
        }
    }

    public GeckoView(final Context context) {
        super(context);
        init();
    }

    public GeckoView(final Context context, final AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    private void init() {
        setFocusable(true);
        setFocusableInTouchMode(true);

        // We are adding descendants to this LayerView, but we don't want the
        // descendants to affect the way LayerView retains its focus.
        setDescendantFocusability(FOCUS_BLOCK_DESCENDANTS);

        // This will stop PropertyAnimator from creating a drawing cache (i.e. a
        // bitmap) from a SurfaceView, which is just not possible (the bitmap will be
        // transparent).
        setWillNotCacheDrawing(false);

        mSurfaceView = new SurfaceView(getContext());
        mSurfaceView.setBackgroundColor(Color.WHITE);
        addView(mSurfaceView,
                new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                                           ViewGroup.LayoutParams.MATCH_PARENT));

        mSurfaceView.getHolder().addCallback(mDisplay);
    }

    /**
     * Set a color to cover the display surface while a document is being shown. The color
     * is automatically cleared once the new document starts painting. Set to
     * Color.TRANSPARENT to undo the cover.
     *
     * @param color Cover color.
     */
    public void coverUntilFirstPaint(final int color) {
        if (mSurfaceView != null) {
            mSurfaceView.setBackgroundColor(color);
        }
    }

    public void setSession(final GeckoSession session) {
        if (mSession != null && mSession.isOpen()) {
            throw new IllegalStateException("Current session is open");
        }

        if (mSession != null) {
            mSession.removeDisplay(mDisplay);
        }
        if (session != null) {
            session.addDisplay(mDisplay);
        }

        final Context context = getContext();
        session.getOverscrollEdgeEffect().setTheme(context);
        session.getOverscrollEdgeEffect().setInvalidationCallback(new Runnable() {
            @Override
            public void run() {
                if (Build.VERSION.SDK_INT >= 16) {
                    GeckoView.this.postInvalidateOnAnimation();
                } else {
                    GeckoView.this.postInvalidateDelayed(10);
                }
            }
        });

        final DisplayMetrics metrics = context.getResources().getDisplayMetrics();
        final TypedValue outValue = new TypedValue();
        if (context.getTheme().resolveAttribute(android.R.attr.listPreferredItemHeight,
                                                outValue, true)) {
            session.getPanZoomController().setScrollFactor(outValue.getDimension(metrics));
        } else {
            session.getPanZoomController().setScrollFactor(0.075f * metrics.densityDpi);
        }

        session.getCompositorController().setFirstPaintCallback(new Runnable() {
            @Override
            public void run() {
                coverUntilFirstPaint(Color.TRANSPARENT);
            }
        });

        mSession = session;
    }

    public GeckoSession getSession() {
        return mSession;
    }

    public EventDispatcher getEventDispatcher() {
        return mSession.getEventDispatcher();
    }

    public GeckoSessionSettings getSettings() {
        return mSession.getSettings();
    }

    public NativePanZoomController getPanZoomController() {
        return mSession.getPanZoomController();
    }

    public DynamicToolbarAnimator getDynamicToolbarAnimator() {
        return mSession.getDynamicToolbarAnimator();
    }

    @Override
    public void onAttachedToWindow() {
        if (mSession == null) {
            setSession(new GeckoSession());
        }

        if (!mSession.isOpen()) {
            mSession.openWindow(getContext().getApplicationContext());
        }
        mSession.attachView(this);

        super.onAttachedToWindow();
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();

        if (mStateSaved) {
            // If we saved state earlier, we don't want to close the window.
            return;
        }

        if (mSession != null && mSession.isOpen()) {
            mSession.closeWindow();
        }
    }

    @Override
    public boolean gatherTransparentRegion(final Region region) {
        // For detecting changes in SurfaceView layout, we take a shortcut here and
        // override gatherTransparentRegion, instead of registering a layout listener,
        // which is more expensive.
        if (mSurfaceView != null) {
            mDisplay.onGlobalLayout();
        }
        return super.gatherTransparentRegion(region);
    }

    @Override
    protected Parcelable onSaveInstanceState() {
        mStateSaved = true;
        return new SavedState(super.onSaveInstanceState(), mSession);
    }

    @Override
    protected void onRestoreInstanceState(final Parcelable state) {
        mStateSaved = false;

        if (!(state instanceof SavedState)) {
            super.onRestoreInstanceState(state);
            return;
        }

        final SavedState ss = (SavedState) state;
        super.onRestoreInstanceState(ss.getSuperState());

        if (mSession == null) {
            setSession(ss.session);
        } else if (ss.session != null) {
            mSession.transferFrom(ss.session);
        }
    }

    /* package */ void setInputConnectionListener(final InputConnectionListener icl) {
        mInputConnectionListener = icl;
    }

    @Override
    public void onFocusChanged(boolean gainFocus, int direction, Rect previouslyFocusedRect) {
        super.onFocusChanged(gainFocus, direction, previouslyFocusedRect);

        if (!gainFocus || mIsResettingFocus) {
            return;
        }

        post(new Runnable() {
            @Override
            public void run() {
                if (!isFocused()) {
                    return;
                }

                final InputMethodManager imm = InputMethods.getInputMethodManager(getContext());
                // Bug 1404111: Through View#onFocusChanged, the InputMethodManager queues
                // up a checkFocus call for the next spin of the message loop, so by
                // posting this Runnable after super#onFocusChanged, the IMM should have
                // completed its focus change handling at this point and we should be the
                // active view for input handling.

                // If however onViewDetachedFromWindow for the previously active view gets
                // called *after* onFocusChanged, but *before* the focus change has been
                // fully processed by the IMM with the help of checkFocus, the IMM will
                // lose track of the currently active view, which means that we can't
                // interact with the IME.
                if (!imm.isActive(GeckoView.this)) {
                    // If that happens, we bring the IMM's internal state back into sync
                    // by clearing and resetting our focus.
                    mIsResettingFocus = true;
                    clearFocus();
                    // After calling clearFocus we might regain focus automatically, but
                    // we explicitly request it again in case this doesn't happen.  If
                    // we've already got the focus back, this will then be a no-op anyway.
                    requestFocus();
                    mIsResettingFocus = false;
                }
            }
        });
    }

    @Override
    public Handler getHandler() {
        if (mInputConnectionListener != null) {
            return mInputConnectionListener.getHandler(super.getHandler());
        }
        return super.getHandler();
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        if (mInputConnectionListener != null) {
            return mInputConnectionListener.onCreateInputConnection(outAttrs);
        }
        return null;
    }

    @Override
    public boolean onKeyPreIme(int keyCode, KeyEvent event) {
        if (super.onKeyPreIme(keyCode, event)) {
            return true;
        }
        return mInputConnectionListener != null &&
                mInputConnectionListener.onKeyPreIme(keyCode, event);
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (super.onKeyUp(keyCode, event)) {
            return true;
        }
        return mInputConnectionListener != null &&
                mInputConnectionListener.onKeyUp(keyCode, event);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (super.onKeyDown(keyCode, event)) {
            return true;
        }
        return mInputConnectionListener != null &&
                mInputConnectionListener.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onKeyLongPress(int keyCode, KeyEvent event) {
        if (super.onKeyLongPress(keyCode, event)) {
            return true;
        }
        return mInputConnectionListener != null &&
                mInputConnectionListener.onKeyLongPress(keyCode, event);
    }

    @Override
    public boolean onKeyMultiple(int keyCode, int repeatCount, KeyEvent event) {
        if (super.onKeyMultiple(keyCode, repeatCount, event)) {
            return true;
        }
        return mInputConnectionListener != null &&
                mInputConnectionListener.onKeyMultiple(keyCode, repeatCount, event);
    }

    @Override
    public void dispatchDraw(final Canvas canvas) {
        super.dispatchDraw(canvas);

        if (mSession != null) {
            mSession.getOverscrollEdgeEffect().draw(canvas);
        }
    }

    @Override
    public boolean onTouchEvent(final MotionEvent event) {
        if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
            requestFocus();
        }

        // NOTE: Treat mouse events as "touch" rather than as "mouse", so mouse can be
        // used to pan/zoom. Call onMouseEvent() instead for behavior similar to desktop.
        return mSession != null &&
               mSession.getPanZoomController().onTouchEvent(event);
    }

    protected static boolean isAccessibilityEnabled(final Context context) {
        if (sAccessibilityManager == null) {
            sAccessibilityManager = (AccessibilityManager)
                    context.getSystemService(Context.ACCESSIBILITY_SERVICE);
        }
        return sAccessibilityManager.isEnabled() &&
               sAccessibilityManager.isTouchExplorationEnabled();
    }

    @Override
    public boolean onHoverEvent(final MotionEvent event) {
        // If we get a touchscreen hover event, and accessibility is not enabled, don't
        // send it to Gecko.
        if (event.getSource() == InputDevice.SOURCE_TOUCHSCREEN &&
            !isAccessibilityEnabled(getContext())) {
            return false;
        }

        return mSession != null &&
               mSession.getPanZoomController().onMotionEvent(event);
    }

    @Override
    public boolean onGenericMotionEvent(final MotionEvent event) {
        if (AndroidGamepadManager.handleMotionEvent(event)) {
            return true;
        }

        return mSession != null &&
               mSession.getPanZoomController().onMotionEvent(event);
    }
}
