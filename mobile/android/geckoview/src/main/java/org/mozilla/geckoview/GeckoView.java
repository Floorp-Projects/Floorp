/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import org.mozilla.gecko.AndroidGamepadManager;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.InputMethods;
import org.mozilla.gecko.SurfaceViewWrapper;
import org.mozilla.gecko.util.ThreadUtils;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.Activity;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Region;
import android.os.Build;
import android.os.Handler;
import androidx.annotation.AnyThread;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.UiThread;
import androidx.core.view.ViewCompat;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.util.Log;
import android.util.SparseArray;
import android.util.TypedValue;
import android.view.DisplayCutout;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceView;
import android.view.TextureView;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStructure;
import android.view.autofill.AutofillManager;
import android.view.autofill.AutofillValue;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;
import android.widget.FrameLayout;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@UiThread
public class GeckoView extends FrameLayout {
    private static final String LOGTAG = "GeckoView";
    private static final boolean DEBUG = false;

    protected final @NonNull Display mDisplay = new Display();

    private Integer mLastCoverColor;
    protected @Nullable GeckoSession mSession;
    private boolean mStateSaved;

    private @Nullable SurfaceViewWrapper mSurfaceWrapper;

    private boolean mIsResettingFocus;

    private boolean mAutofillEnabled = true;

    private GeckoSession.SelectionActionDelegate mSelectionActionDelegate;
    private Autofill.Delegate mAutofillDelegate;

    private class Display implements SurfaceViewWrapper.Listener {
        private final int[] mOrigin = new int[2];

        private GeckoDisplay mDisplay;
        private boolean mValid;

        private int mClippingHeight;
        private int mDynamicToolbarMaxHeight;

        public void acquire(final GeckoDisplay display) {
            mDisplay = display;

            if (!mValid) {
                return;
            }

            setVerticalClipping(mClippingHeight);

            // Tell display there is already a surface.
            onGlobalLayout();
            if (GeckoView.this.mSurfaceWrapper != null) {
                final SurfaceViewWrapper wrapper = GeckoView.this.mSurfaceWrapper;
                mDisplay.surfaceChanged(wrapper.getSurface(),
                        wrapper.getWidth(), wrapper.getHeight());
                mDisplay.setDynamicToolbarMaxHeight(mDynamicToolbarMaxHeight);
                GeckoView.this.setActive(true);
            }
        }

        public GeckoDisplay release() {
            if (mValid) {
                if (mDisplay != null) {
                    mDisplay.surfaceDestroyed();
                }
                GeckoView.this.setActive(false);
            }

            final GeckoDisplay display = mDisplay;
            mDisplay = null;
            return display;
        }

        @Override // SurfaceListener
        public void onSurfaceChanged(final Surface surface,
                                   final int width, final int height) {
            if (mDisplay != null) {
                mDisplay.surfaceChanged(surface, width, height);
                mDisplay.setDynamicToolbarMaxHeight(mDynamicToolbarMaxHeight);
                if (!mValid) {
                    GeckoView.this.setActive(true);
                }
            }
            mValid = true;
        }

        @Override // SurfaceListener
        public void onSurfaceDestroyed() {
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
            if (GeckoView.this.mSurfaceWrapper != null) {
                GeckoView.this.mSurfaceWrapper.getView().getLocationOnScreen(mOrigin);
                mDisplay.screenOriginChanged(mOrigin[0], mOrigin[1]);
                // cutout support
                if (Build.VERSION.SDK_INT >= 28) {
                    final DisplayCutout cutout = GeckoView.this.mSurfaceWrapper.getView().getRootWindowInsets().getDisplayCutout();
                    if (cutout != null) {
                        mDisplay.safeAreaInsetsChanged(cutout.getSafeInsetTop(), cutout.getSafeInsetRight(), cutout.getSafeInsetBottom(), cutout.getSafeInsetLeft());
                    }
                }
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

        public void setDynamicToolbarMaxHeight(final int height) {
            mDynamicToolbarMaxHeight = height;

            // Reset the vertical clipping value to zero whenever we change
            // the dynamic toolbar __max__ height so that it can be properly
            // propagated to both the main thread and the compositor thread,
            // thus we will be able to reset the __current__ toolbar height
            // on the both threads whatever the __current__ toolbar height is.
            setVerticalClipping(0);

            if (mDisplay != null) {
                mDisplay.setDynamicToolbarMaxHeight(height);
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

    @SuppressWarnings("checkstyle:javadocmethod")
    public GeckoView(final Context context) {
        super(context);
        init();
    }

    @SuppressWarnings("checkstyle:javadocmethod")
    public GeckoView(final Context context, final AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    private static Activity getActivityFromContext(final Context outerContext) {
        Context context = outerContext;
        while (context instanceof ContextWrapper) {
            if (context instanceof Activity) {
                return (Activity) context;
            }
            context = ((ContextWrapper) context).getBaseContext();
        }
        return null;
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

        mSurfaceWrapper = new SurfaceViewWrapper(getContext());
        mSurfaceWrapper.setBackgroundColor(Color.WHITE);
        addView(mSurfaceWrapper.getView(),
                new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                                           ViewGroup.LayoutParams.MATCH_PARENT));

        mSurfaceWrapper.setListener(mDisplay);

        final Activity activity = getActivityFromContext(getContext());
        if (activity != null) {
            mSelectionActionDelegate = new BasicSelectionActionDelegate(activity);
        }

        mAutofillDelegate = new AndroidAutofillDelegate();
    }

    /**
     * Set a color to cover the display surface while a document is being shown. The color
     * is automatically cleared once the new document starts painting.
     *
     * @param color Cover color.
     */
    public void coverUntilFirstPaint(final int color) {
        mLastCoverColor = color;
        if (mSession != null) {
            mSession.getCompositorController().setClearColor(color);
        }
        coverUntilFirstPaintInternal(color);
    }

    private void uncover() {
        coverUntilFirstPaintInternal(Color.TRANSPARENT);
    }

    private void coverUntilFirstPaintInternal(final int color) {
        ThreadUtils.assertOnUiThread();

        if (mSurfaceWrapper != null) {
            mSurfaceWrapper.setBackgroundColor(color);
        }
    }

    /**
     * This GeckoView instance will be backed by a {@link SurfaceView}.
     *
     * This option offers the best performance at the price of not being
     * able to animate GeckoView.
     */
    public static final int BACKEND_SURFACE_VIEW = 1;
    /**
     * This GeckoView instance will be backed by a {@link TextureView}.
     *
     * This option offers worse performance compared to {@link #BACKEND_SURFACE_VIEW}
     * but allows you to animate GeckoView or to paint a GeckoView on top of another GeckoView.
     */
    public static final int BACKEND_TEXTURE_VIEW = 2;

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({BACKEND_SURFACE_VIEW, BACKEND_TEXTURE_VIEW})
    /* protected */ @interface ViewBackend {}

    /**
     * Set which view should be used by this GeckoView instance to display content.
     *
     * By default, GeckoView will use a {@link SurfaceView}.
     *
     * @param backend Any of {@link #BACKEND_SURFACE_VIEW BACKEND_*}.
     */
    public void setViewBackend(final @ViewBackend int backend) {
        removeView(mSurfaceWrapper.getView());

        if (backend == BACKEND_SURFACE_VIEW) {
            mSurfaceWrapper.useSurfaceView(getContext());
        } else if (backend == BACKEND_TEXTURE_VIEW) {
            mSurfaceWrapper.useTextureView(getContext());
        }

        addView(mSurfaceWrapper.getView());
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

    /**
     * Set the maximum height of the dynamic toolbar(s).
     *
     * If there are two or more dynamic toolbars, the height value should be the total amount of
     * the height of each dynamic toolbar.
     *
     * @param height The the maximum height of the dynamic toolbar(s).
     */
    public void setDynamicToolbarMaxHeight(final int height) {
        mDisplay.setDynamicToolbarMaxHeight(height);
    }

    /* package */ void setActive(final boolean active) {
        if (mSession != null) {
            mSession.setActive(active);
        }
    }

    // TODO: Bug 1670805 this should really be configurable
    // Default dark color for about:blank, keep it in sync with PresShell.cpp
    final static int DEFAULT_DARK_COLOR = 0xFF2A2A2E;

    private int defaultColor() {
        // If the app set a default color, just use that
        if (mLastCoverColor != null) {
            return mLastCoverColor;
        }

        if (mSession == null || !mSession.isOpen()) {
            return Color.WHITE;
        }

        // ... otherwise use the prefers-color-scheme color
        return mSession.getRuntime().usesDarkTheme() ?
                DEFAULT_DARK_COLOR : Color.WHITE;
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

        final GeckoSession session = mSession;
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

        if (mSession.getAutofillDelegate() == mAutofillDelegate) {
            mSession.setAutofillDelegate(null);
        }

        if (isFocused()) {
            mSession.setFocused(false);
        }
        mSession = null;
        return session;
    }

    /**
     * Attach a session to this view. If this instance already has an open session, you must use
     * {@link #releaseSession()} first, otherwise {@link IllegalStateException}
     * will be thrown. This is to avoid potentially leaking the currently opened session.
     *
     * @param session The session to be attached.
     * @throws IllegalArgumentException if an existing open session is already set.
     */
    @UiThread
    public void setSession(@NonNull final GeckoSession session) {
        ThreadUtils.assertOnUiThread();

        if (mSession != null && mSession.isOpen()) {
            throw new IllegalStateException("Current session is open");
        }

        releaseSession();

        mSession = session;

        // Make sure the clear color is set to the default
        mSession.getCompositorController()
                .setClearColor(defaultColor());

        if (ViewCompat.isAttachedToWindow(this)) {
            mDisplay.acquire(session.acquireDisplay());
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

        session.getCompositorController().setFirstPaintCallback(this::uncover);

        if (session.getTextInput().getView() == null) {
            session.getTextInput().setView(this);
        }

        if (session.getAccessibility().getView() == null) {
            session.getAccessibility().setView(this);
        }

        if (session.getSelectionActionDelegate() == null && mSelectionActionDelegate != null) {
            session.setSelectionActionDelegate(mSelectionActionDelegate);
        }

        if (mAutofillEnabled) {
            session.setAutofillDelegate(mAutofillDelegate);
        }

        if (isFocused()) {
            session.setFocused(true);
        }
    }

    @AnyThread
    @SuppressWarnings("checkstyle:javadocmethod")
    public @Nullable GeckoSession getSession() {
        return mSession;
    }

    @AnyThread
    /* package */ @NonNull EventDispatcher getEventDispatcher() {
        return mSession.getEventDispatcher();
    }

    @SuppressWarnings("checkstyle:javadocmethod")
    public @NonNull PanZoomController getPanZoomController() {
        ThreadUtils.assertOnUiThread();
        return mSession.getPanZoomController();
    }

    @Override
    public void onAttachedToWindow() {
        if (mSession != null) {
            final GeckoRuntime runtime = mSession.getRuntime();
            if (runtime != null) {
                runtime.orientationChanged();
            }
        }

        if (mSession != null) {
            mDisplay.acquire(mSession.acquireDisplay());
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
    }

    @Override
    protected void onConfigurationChanged(final Configuration newConfig) {
        super.onConfigurationChanged(newConfig);

        if (mSession != null) {
            final GeckoRuntime runtime = mSession.getRuntime();
            if (runtime != null) {
                // onConfigurationChanged is not called for 180 degree orientation changes,
                // we will miss such rotations and the screen orientation will not be
                // updated.
                runtime.orientationChanged(newConfig.orientation);
                runtime.configurationChanged(newConfig);
            }
        }
    }

    @Override
    public boolean gatherTransparentRegion(final Region region) {
        // For detecting changes in SurfaceView layout, we take a shortcut here and
        // override gatherTransparentRegion, instead of registering a layout listener,
        // which is more expensive.
        if (mSurfaceWrapper != null) {
            mDisplay.onGlobalLayout();
        }
        return super.gatherTransparentRegion(region);
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

        if (mSession == null) {
            return false;
        }

        mSession.getPanZoomController().onTouchEvent(event);
        return true;
    }

    /**
     * @deprecated Use {@link #onTouchEventForDetailResult(MotionEvent)} instead.
     *
     * Dispatches a {@link MotionEvent} to the {@link PanZoomController}. This is the same as
     * {@link #onTouchEvent(MotionEvent)}, but instead returns a {@link PanZoomController.InputResult}
     * indicating how the event was handled.
     *
     * NOTE: It is highly recommended to only call this with ACTION_DOWN or in otherwise
     * limited capacity. Returning a GeckoResult for every touch event will generate
     * a lot of allocations and unnecessary GC pressure.
     *
     * @param event A {@link MotionEvent}
     * @return One of the {@link PanZoomController#INPUT_RESULT_UNHANDLED INPUT_RESULT_*} indicating how the event was handled.
     */
    @Deprecated @DeprecationSchedule(version = 90, id = "on-touch-event-for-result")
    public @NonNull GeckoResult<Integer> onTouchEventForResult(final @NonNull MotionEvent event) {
        if (mSession == null) {
            return GeckoResult.fromValue(PanZoomController.INPUT_RESULT_UNHANDLED);
        }

        return mSession.getPanZoomController().onTouchEventForDetailResult(event)
                       .map(detail -> detail.handledResult());
    }

    /**
     * Dispatches a {@link MotionEvent} to the {@link PanZoomController}. This is the same as
     * {@link #onTouchEvent(MotionEvent)}, but instead returns a {@link PanZoomController.InputResult}
     * indicating how the event was handled.
     *
     * NOTE: It is highly recommended to only call this with ACTION_DOWN or in otherwise
     * limited capacity. Returning a GeckoResult for every touch event will generate
     * a lot of allocations and unnecessary GC pressure.
     *
     * @param event A {@link MotionEvent}
     * @return A GeckoResult resolving to {@link PanZoomController.InputResultDetail}.
     */
    public @NonNull GeckoResult<PanZoomController.InputResultDetail> onTouchEventForDetailResult(final @NonNull MotionEvent event) {
        if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
            requestFocus();
        }

        if (mSession == null) {
            return GeckoResult.fromValue(
                new PanZoomController.InputResultDetail(PanZoomController.INPUT_RESULT_UNHANDLED,
                                                        PanZoomController.SCROLLABLE_FLAG_NONE,
                                                        PanZoomController.OVERSCROLL_FLAG_NONE));
        }

        // NOTE: Treat mouse events as "touch" rather than as "mouse", so mouse can be
        // used to pan/zoom. Call onMouseEvent() instead for behavior similar to desktop.
        return mSession.getPanZoomController().onTouchEventForDetailResult(event);
    }

    @Override
    public boolean onGenericMotionEvent(final MotionEvent event) {
        if (AndroidGamepadManager.handleMotionEvent(event)) {
            return true;
        }

        if (mSession == null) {
            return true;
        }

        if (mSession.getAccessibility().onMotionEvent(event)) {
            return true;
        }

        mSession.getPanZoomController().onMotionEvent(event);
        return true;
    }

    @Override
    public void onProvideAutofillVirtualStructure(final ViewStructure structure,
                                                  final int flags) {
        super.onProvideAutofillVirtualStructure(structure, flags);

        if (mSession == null) {
            return;
        }

        final Autofill.Session autofillSession = mSession.getAutofillSession();
        autofillSession.fillViewStructure(this, structure, flags);
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
        mSession.autofill(strValues);
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

    /**
     * Sets whether or not this View participates in Android autofill.
     *
     * When enabled, this will set an {@link Autofill.Delegate} on the
     * {@link GeckoSession} for this instance.
     *
     * @param enabled Whether or not Android autofill is enabled for this view.
     */
    @TargetApi(26)
    public void setAutofillEnabled(final boolean enabled) {
        mAutofillEnabled = enabled;

        if (mSession != null) {
            if (!enabled && mSession.getAutofillDelegate() == mAutofillDelegate) {
                mSession.setAutofillDelegate(null);
            } else if (enabled) {
                mSession.setAutofillDelegate(mAutofillDelegate);
            }
        }
    }

    /**
     * @return Whether or not Android autofill is enabled for this view.
     */
    @TargetApi(26)
    public boolean getAutofillEnabled() {
        return mAutofillEnabled;
    }

    private class AndroidAutofillDelegate implements Autofill.Delegate {

        private Rect displayRectForId(@NonNull final GeckoSession session,
                                      @NonNull final Autofill.Node node) {
            if (node == null) {
                return new Rect(0, 0, 0, 0);
            }

            final Matrix matrix = new Matrix();
            final RectF rectF = new RectF(node.getDimensions());
            session.getPageToScreenMatrix(matrix);
            matrix.mapRect(rectF);

            final Rect screenRect = new Rect();
            rectF.roundOut(screenRect);
            return screenRect;
        }

        @Override
        public void onAutofill(@NonNull final GeckoSession session,
                               final int notification,
                               final Autofill.Node node) {
            ThreadUtils.assertOnUiThread();
            if (Build.VERSION.SDK_INT < 26) {
                return;
            }

            final AutofillManager manager =
                    GeckoView.this.getContext().getSystemService(AutofillManager.class);
            if (manager == null) {
                return;
            }

            try {
                switch (notification) {
                    case Autofill.Notify.SESSION_STARTED:
                        // This line seems necessary for auto-fill to work on the initial page.
                    case Autofill.Notify.SESSION_CANCELED:
                        manager.cancel();
                        break;
                    case Autofill.Notify.SESSION_COMMITTED:
                        manager.commit();
                        break;
                    case Autofill.Notify.NODE_FOCUSED:
                        manager.notifyViewEntered(
                                GeckoView.this, node.getId(),
                                displayRectForId(session, node));
                        break;
                    case Autofill.Notify.NODE_BLURRED:
                        manager.notifyViewExited(GeckoView.this, node.getId());
                        break;
                    case Autofill.Notify.NODE_UPDATED:
                        manager.notifyValueChanged(
                                GeckoView.this,
                                node.getId(),
                                AutofillValue.forText(node.getValue()));
                        break;
                }
            } catch (final SecurityException e) {
                Log.e(LOGTAG, "Failed to call Autofill Manager API: ", e);
            }
        }
    }
}
