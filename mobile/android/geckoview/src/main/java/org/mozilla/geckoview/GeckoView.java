/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import org.mozilla.gecko.AndroidGamepadManager;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.InputMethods;
import org.mozilla.gecko.util.ActivityUtils;
import org.mozilla.gecko.util.ThreadUtils;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.Region;
import android.os.Build;
import android.os.Handler;
import android.os.Parcel;
import android.os.Parcelable;
import android.support.annotation.AnyThread;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.UiThread;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.util.Log;
import android.util.SparseArray;
import android.util.TypedValue;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStructure;
import android.view.autofill.AutofillValue;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;
import android.widget.FrameLayout;

@UiThread
public class GeckoView extends FrameLayout {
    private static final String LOGTAG = "GeckoView";
    private static final boolean DEBUG = false;

    protected final @NonNull Display mDisplay = new Display();
    protected @Nullable GeckoSession mSession;
    protected @Nullable GeckoRuntime mRuntime;
    private boolean mStateSaved;

    protected @Nullable SurfaceView mSurfaceView;

    private boolean mIsResettingFocus;

    private GeckoSession.SelectionActionDelegate mSelectionActionDelegate;

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

    private class Display implements SurfaceHolder.Callback {
        private final int[] mOrigin = new int[2];

        private GeckoDisplay mDisplay;
        private boolean mValid;

        private int mClippingHeight;

        public void acquire(final GeckoDisplay display) {
            mDisplay = display;

            if (!mValid) {
                return;
            }

            setVerticalClipping(mClippingHeight);

            // Tell display there is already a surface.
            onGlobalLayout();
            if (GeckoView.this.mSurfaceView != null) {
                final SurfaceHolder holder = GeckoView.this.mSurfaceView.getHolder();
                final Rect frame = holder.getSurfaceFrame();
                mDisplay.surfaceChanged(holder.getSurface(), frame.right, frame.bottom);
                GeckoView.this.setActive(true);
            }
        }

        public GeckoDisplay release() {
            if (mValid) {
                mDisplay.surfaceDestroyed();
                GeckoView.this.setActive(false);
            }

            final GeckoDisplay display = mDisplay;
            mDisplay = null;
            return display;
        }

        @Override // SurfaceHolder.Callback
        public void surfaceCreated(final SurfaceHolder holder) {
        }

        @Override // SurfaceHolder.Callback
        public void surfaceChanged(final SurfaceHolder holder, final int format,
                                   final int width, final int height) {
            if (mDisplay != null) {
                mDisplay.surfaceChanged(holder.getSurface(), width, height);
                if (!mValid) {
                    GeckoView.this.setActive(true);
                }
            }
            mValid = true;
        }

        @Override // SurfaceHolder.Callback
        public void surfaceDestroyed(final SurfaceHolder holder) {
            if (mDisplay != null) {
                mDisplay.surfaceDestroyed();
                GeckoView.this.setActive(false);
            }
            mValid = false;
        }

        public void onGlobalLayout() {
            if (mDisplay == null) {
                return;
            }
            if (GeckoView.this.mSurfaceView != null) {
                GeckoView.this.mSurfaceView.getLocationOnScreen(mOrigin);
                mDisplay.screenOriginChanged(mOrigin[0], mOrigin[1]);
            }
        }

        public boolean shouldPinOnScreen() {
            return mDisplay != null ? mDisplay.shouldPinOnScreen() : false;
        }

        public void setVerticalClipping(final int clippingHeight) {
            mClippingHeight = clippingHeight;

            if (mDisplay != null) {
                mDisplay.setVerticalClipping(clippingHeight);
            }
        }

        /**
         * Request a {@link Bitmap} of the visible portion of the web page currently being
         * rendered.
         *
         * @return A {@link GeckoResult} that completes with a {@link Bitmap} containing
         * the pixels and size information of the currently visible rendered web page.
         */
        @UiThread
        @NonNull GeckoResult<Bitmap> capturePixels() {
            if (mDisplay == null) {
                return GeckoResult.fromException(new IllegalStateException("Display must be created before pixels can be captured"));
            }

            return mDisplay.capturePixels();
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
        setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_YES);

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

        final Activity activity = ActivityUtils.getActivityFromContext(getContext());
        if (activity != null) {
            mSelectionActionDelegate = new BasicSelectionActionDelegate(activity);
        }
    }

    /**
     * Set a color to cover the display surface while a document is being shown. The color
     * is automatically cleared once the new document starts painting. Set to
     * Color.TRANSPARENT to undo the cover.
     *
     * @param color Cover color.
     */
    public void coverUntilFirstPaint(final int color) {
        ThreadUtils.assertOnUiThread();

        if (mSurfaceView != null) {
            mSurfaceView.setBackgroundColor(color);
        }
    }

    /**
     * Return whether the view should be pinned on the screen. When pinned, the view
     * should not be moved on the screen due to animation, scrolling, etc. A common reason
     * for the view being pinned is when the user is dragging a selection caret inside
     * the view; normal user interaction would be disrupted in that case if the view
     * was moved on screen.
     *
     * @return True if view should be pinned on the screen.
     */
    public boolean shouldPinOnScreen() {
        ThreadUtils.assertOnUiThread();

        return mDisplay.shouldPinOnScreen();
    }

    /**
     * Update the amount of vertical space that is clipped or visibly obscured in the bottom portion
     * of the view. Tells gecko where to put bottom fixed elements so they are fully visible.
     *
     * Optional call. The display's visible vertical space has changed. Must be
     * called on the application main thread.
     *
     * @param clippingHeight The height of the bottom clipped space in screen pixels.
     */
    public void setVerticalClipping(final int clippingHeight) {
        ThreadUtils.assertOnUiThread();

        mDisplay.setVerticalClipping(clippingHeight);
    }

    /* package */ void setActive(final boolean active) {
        if (mSession != null) {
            mSession.setActive(active);
        }
    }

    /**
     * Unsets the current session from this instance and returns it, if any. You must call
     * this before {@link #setSession(GeckoSession)} if there is already an open session
     * set for this instance.
     *
     * Note: this method does not close the session and the session remains active. The
     * caller is responsible for calling {@link GeckoSession#close()} when appropriate.
     *
     * @return The {@link GeckoSession} that was set for this instance. May be null.
     */
    @UiThread
    public @Nullable GeckoSession releaseSession() {
        ThreadUtils.assertOnUiThread();

        if (mSession == null) {
            return null;
        }

        // Cover the view while we are not drawing to the surface.
        coverUntilFirstPaint(Color.WHITE);

        GeckoSession session = mSession;
        mSession.releaseDisplay(mDisplay.release());
        mSession.getOverscrollEdgeEffect().setInvalidationCallback(null);
        mSession.getCompositorController().setFirstPaintCallback(null);

        if (mSession.getAccessibility().getView() == this) {
            mSession.getAccessibility().setView(null);
        }

        if (mSession.getTextInput().getView() == this) {
            mSession.getTextInput().setView(null);
        }

        if (mSession.getSelectionActionDelegate() == mSelectionActionDelegate) {
            mSession.setSelectionActionDelegate(null);
        }

        if (isFocused()) {
            mSession.setFocused(false);
        }
        mSession = null;
        mRuntime = null;
        return session;
    }

    /**
     * Attach a session to this view. The session should be opened before
     * attaching. If this instance already has an open session, you must use
     * {@link #releaseSession()} first, otherwise {@link IllegalStateException}
     * will be thrown. This is to avoid potentially leaking the currently opened session.
     *
     * @param session The session to be attached.
     * @throws IllegalArgumentException if an existing open session is already set.
     */
    @UiThread
    public void setSession(@NonNull final GeckoSession session) {
        ThreadUtils.assertOnUiThread();

        if (!session.isOpen()) {
            throw new IllegalArgumentException("Session must be open before attaching");
        }

        setSession(session, session.getRuntime());
    }

    /**
     * Attach a session to this view. The session should be opened before
     * attaching or a runtime needs to be provided for automatic opening.
     * If this instance already has an open session, you must use
     * {@link #releaseSession()} first, otherwise {@link IllegalStateException}
     * will be thrown. This is to avoid potentially leaking the currently opened session.
     *
     * @param session The session to be attached.
     * @param runtime The runtime to be used for opening the session.
     * @throws IllegalArgumentException if an existing open session is already set.
     */
    @UiThread
    public void setSession(@NonNull final GeckoSession session,
                           @Nullable final GeckoRuntime runtime) {
        ThreadUtils.assertOnUiThread();

        if (mSession != null && mSession.isOpen()) {
            throw new IllegalStateException("Current session is open");
        }

        releaseSession();

        mSession = session;
        mRuntime = runtime;

        if (session.isOpen()) {
            if (runtime != null && runtime != session.getRuntime()) {
                throw new IllegalArgumentException("Session was opened with non-matching runtime");
            }
            mRuntime = session.getRuntime();
        } else if (runtime == null) {
            throw new IllegalArgumentException("Session must be open before attaching");
        }

        mDisplay.acquire(session.acquireDisplay());

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

        if (session.getTextInput().getView() == null) {
            session.getTextInput().setView(this);
        }

        if (session.getAccessibility().getView() == null) {
            session.getAccessibility().setView(this);
        }

        if (session.getSelectionActionDelegate() == null && mSelectionActionDelegate != null) {
            session.setSelectionActionDelegate(mSelectionActionDelegate);
        }

        if (isFocused()) {
            session.setFocused(true);
        }
    }

    @AnyThread
    public @Nullable GeckoSession getSession() {
        return mSession;
    }

    @AnyThread
    /* package */ @NonNull EventDispatcher getEventDispatcher() {
        return mSession.getEventDispatcher();
    }

    public @NonNull PanZoomController getPanZoomController() {
        ThreadUtils.assertOnUiThread();
        return mSession.getPanZoomController();
    }

    public @NonNull DynamicToolbarAnimator getDynamicToolbarAnimator() {
        ThreadUtils.assertOnUiThread();
        return mSession.getDynamicToolbarAnimator();
    }

    @Override
    public void onAttachedToWindow() {
        if (mSession != null && mRuntime != null) {
            if (!mSession.isOpen()) {
                mSession.open(mRuntime);
            }
            mRuntime.orientationChanged();
        } else {
            Log.w(LOGTAG, "No GeckoSession attached to this GeckoView instance. Call setSession to attach a GeckoSession to this instance.");
        }

        super.onAttachedToWindow();
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();

        if (mSession == null) {
            return;
        }

        // Release the display before we detach from the window.
        mSession.releaseDisplay(mDisplay.release());

        // If we saved state earlier, we don't want to close the window.
        if (!mStateSaved && mSession.isOpen()) {
            mSession.close();
        }

    }

    @Override
    protected void onConfigurationChanged(final Configuration newConfig) {
        super.onConfigurationChanged(newConfig);

        if (mRuntime != null) {
            // onConfigurationChanged is not called for 180 degree orientation changes,
            // we will miss such rotations and the screen orientation will not be
            // updated.
            mRuntime.orientationChanged(newConfig.orientation);
            mRuntime.configurationChanged(newConfig);
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

        restoreSession(ss.session);
    }

    private void restoreSession(final @Nullable GeckoSession savedSession) {
        if (savedSession == null || savedSession.equals(mSession)) {
            return;
        }

        GeckoRuntime runtimeToRestore = savedSession.getRuntime();
        // Note: setSession sets either both mSession and mRuntime, or none of them. So if we don't
        // have an mRuntime here, we won't have an mSession, either.
        if (mRuntime == null) {
            if (runtimeToRestore == null) {
                // If the saved session is closed, we fall back to using the default runtime, same
                // as we do when we don't even have an mSession in onAttachedToWindow().
                runtimeToRestore = GeckoRuntime.getDefault(getContext());
            }
            setSession(savedSession, runtimeToRestore);
        // We already have a session. We only want to transfer the saved session if its close/open
        // state is the same or better as our current session.
        } else if (savedSession.isOpen() || !mSession.isOpen()) {
            if (mSession.isOpen()) {
                mSession.close();
            }
            mSession.transferFrom(savedSession);
            if (runtimeToRestore != null) {
                // If the saved session was open, we transfer its runtime as well. Otherwise we just
                // keep the runtime we already had in mRuntime.
                mRuntime = runtimeToRestore;
            }
        }
    }

    @Override
    public void onWindowFocusChanged(final boolean hasWindowFocus) {
        super.onWindowFocusChanged(hasWindowFocus);

        // Only call setFocus(true) when the window gains focus. Any focus loss could be temporary
        // (e.g. due to auto-fill popups) and we don't want to call setFocus(false) in those cases.
        // Instead, we call setFocus(false) in onWindowVisibilityChanged.
        if (mSession != null && hasWindowFocus && isFocused()) {
            mSession.setFocused(true);
        }
    }

    @Override
    protected void onWindowVisibilityChanged(final int visibility) {
        super.onWindowVisibilityChanged(visibility);

        // We can be reasonably sure that the focus loss is not temporary, so call setFocus(false).
        if (mSession != null && visibility != View.VISIBLE && !hasWindowFocus()) {
            mSession.setFocused(false);
        }
    }

    @Override
    protected void onFocusChanged(final boolean gainFocus, final int direction,
                                  final Rect previouslyFocusedRect) {
        super.onFocusChanged(gainFocus, direction, previouslyFocusedRect);

        if (mIsResettingFocus) {
            return;
        }

        if (mSession != null) {
            mSession.setFocused(gainFocus);
        }

        if (!gainFocus) {
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
        if (Build.VERSION.SDK_INT >= 24 || mSession == null) {
            return super.getHandler();
        }
        return mSession.getTextInput().getHandler(super.getHandler());
    }

    @Override
    public InputConnection onCreateInputConnection(final EditorInfo outAttrs) {
        if (mSession == null) {
            return null;
        }
        return mSession.getTextInput().onCreateInputConnection(outAttrs);
    }

    @Override
    public boolean onKeyPreIme(final int keyCode, final KeyEvent event) {
        if (super.onKeyPreIme(keyCode, event)) {
            return true;
        }
        return mSession != null &&
               mSession.getTextInput().onKeyPreIme(keyCode, event);
    }

    @Override
    public boolean onKeyUp(final int keyCode, final KeyEvent event) {
        if (super.onKeyUp(keyCode, event)) {
            return true;
        }
        return mSession != null &&
               mSession.getTextInput().onKeyUp(keyCode, event);
    }

    @Override
    public boolean onKeyDown(final int keyCode, final KeyEvent event) {
        if (super.onKeyDown(keyCode, event)) {
            return true;
        }
        return mSession != null &&
               mSession.getTextInput().onKeyDown(keyCode, event);
    }

    @Override
    public boolean onKeyLongPress(final int keyCode, final KeyEvent event) {
        if (super.onKeyLongPress(keyCode, event)) {
            return true;
        }
        return mSession != null &&
               mSession.getTextInput().onKeyLongPress(keyCode, event);
    }

    @Override
    public boolean onKeyMultiple(final int keyCode, final int repeatCount, final KeyEvent event) {
        if (super.onKeyMultiple(keyCode, repeatCount, event)) {
            return true;
        }
        return mSession != null &&
               mSession.getTextInput().onKeyMultiple(keyCode, repeatCount, event);
    }

    @Override
    public void dispatchDraw(final Canvas canvas) {
        super.dispatchDraw(canvas);

        if (mSession != null) {
            mSession.getOverscrollEdgeEffect().draw(canvas);
        }
    }

    @SuppressLint("ClickableViewAccessibility")
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

    @Override
    public boolean onGenericMotionEvent(final MotionEvent event) {
        if (AndroidGamepadManager.handleMotionEvent(event)) {
            return true;
        }

        if (mSession == null) {
            return false;
        }

        return mSession.getAccessibility().onMotionEvent(event) ||
               mSession.getPanZoomController().onMotionEvent(event);
    }

    @Override
    public void onProvideAutofillVirtualStructure(final ViewStructure structure,
                                                  final int flags) {
        super.onProvideAutofillVirtualStructure(structure, flags);

        if (mSession != null) {
            mSession.getTextInput().onProvideAutofillVirtualStructure(structure, flags);
        }
    }

    @Override
    @TargetApi(26)
    public void autofill(@NonNull final SparseArray<AutofillValue> values) {
        super.autofill(values);

        if (mSession == null) {
            return;
        }
        final SparseArray<CharSequence> strValues = new SparseArray<>(values.size());
        for (int i = 0; i < values.size(); i++) {
            final AutofillValue value = values.valueAt(i);
            if (value.isText()) {
                // Only text is currently supported.
                strValues.put(values.keyAt(i), value.getTextValue());
            }
        }
        mSession.getTextInput().autofill(strValues);
    }

    /**
     * Request a {@link Bitmap} of the visible portion of the web page currently being
     * rendered.
     *
     * See {@link GeckoDisplay#capturePixels} for more details.
     *
     * @return A {@link GeckoResult} that completes with a {@link Bitmap} containing
     * the pixels and size information of the currently visible rendered web page.
     */
    @UiThread
    public @NonNull GeckoResult<Bitmap> capturePixels() {
        return mDisplay.capturePixels();
    }
}
