/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.PrefsHelper;
import org.mozilla.gecko.util.FloatUtils;
import org.mozilla.gecko.util.ThreadUtils;

import android.graphics.PointF;
import android.support.v4.view.ViewCompat;
import android.util.Log;
import android.view.MotionEvent;
import android.view.animation.LinearInterpolator;

import java.util.ArrayList;
import java.util.Collections;
import java.util.EnumSet;
import java.util.List;
import java.util.Set;

public class DynamicToolbarAnimator {
    private static final String LOGTAG = "GeckoDynamicToolbarAnimator";
    private static final String PREF_SCROLL_TOOLBAR_THRESHOLD = "browser.ui.scroll-toolbar-threshold";

    public static enum PinReason {
        RELAYOUT,
        ACTION_MODE,
        FULL_SCREEN,
        CARET_DRAG
    }

    private final Set<PinReason> pinFlags = Collections.synchronizedSet(EnumSet.noneOf(PinReason.class));

    // The duration of the animation in ns
    private static final long ANIMATION_DURATION = 150000000;

    private final GeckoLayerClient mTarget;
    private final List<LayerView.DynamicToolbarListener> mListeners;

    /* The translation to be applied to the toolbar UI view. This is the
     * distance from the default/initial location (at the top of the screen,
     * visible to the user) to where we want it to be. This variable should
     * always be between 0 (toolbar fully visible) and the height of the toolbar
     * (toolbar fully hidden), inclusive.
     */
    private float mToolbarTranslation;

    /* The translation to be applied to the LayerView. This is the distance from
     * the default/initial location (just below the toolbar, with the bottom
     * extending past the bottom of the screen) to where we want it to be.
     * This variable should always be between 0 and the height of the toolbar,
     * inclusive.
     */
    private float mLayerViewTranslation;

    /* This stores the maximum translation that can be applied to the toolbar
     * and layerview when scrolling. This is populated with the height of the
     * toolbar. */
    private float mMaxTranslation;

    /* This interpolator is used for the above mentioned animation */
    private LinearInterpolator mInterpolator;

    /* This is the proportion of the viewport rect that needs to be travelled
     * while scrolling before the translation will start taking effect.
     */
    private float SCROLL_TOOLBAR_THRESHOLD = 0.20f;
    /* The ID of the prefs listener for the scroll-toolbar threshold */
    private final PrefsHelper.PrefHandler mPrefObserver;

    /* While we are resizing the viewport to account for the toolbar, the Java
     * code and painted layer metrics in the compositor have different notions
     * of the CSS viewport height. The Java value is stored in the
     * GeckoLayerClient's viewport metrics, and the Gecko one is stored here.
     * This allows us to adjust fixed-pos items correctly.
     * You must synchronize on mTarget.getLock() to read/write this. */
    private Integer mHeightDuringResize;

    /* This tracks if we should trigger a "snap" on the next composite. A "snap"
     * is when we simultaneously move the LayerView and change the scroll offset
     * in the compositor so that everything looks the same on the screen but
     * has really been shifted.
     * You must synchronize on |this| to read/write this. */
    private boolean mSnapRequired = false;

    /* The task that handles showing/hiding toolbar */
    private DynamicToolbarAnimationTask mAnimationTask;

    /* The start point of a drag, used for scroll-based dynamic toolbar
     * behaviour. */
    private PointF mTouchStart;
    private float mLastTouch;

    /* Set to true when root content is being scrolled */
    private boolean mScrollingRootContent;

    public DynamicToolbarAnimator(GeckoLayerClient aTarget) {
        mTarget = aTarget;
        mListeners = new ArrayList<LayerView.DynamicToolbarListener>();

        mInterpolator = new LinearInterpolator();

        // Listen to the dynamic toolbar pref
        mPrefObserver = new PrefsHelper.PrefHandlerBase() {
            @Override
            public void prefValue(String pref, int value) {
                SCROLL_TOOLBAR_THRESHOLD = value / 100.0f;
            }
        };
        PrefsHelper.addObserver(new String[] { PREF_SCROLL_TOOLBAR_THRESHOLD }, mPrefObserver);

        // JPZ doesn't notify when scrolling root content. This maintains existing behaviour.
        if (!AppConstants.MOZ_ANDROID_APZ) {
            mScrollingRootContent = true;
        }
    }

    public void destroy() {
        PrefsHelper.removeObserver(mPrefObserver);
    }

    public void addTranslationListener(LayerView.DynamicToolbarListener aListener) {
        mListeners.add(aListener);
    }

    public void removeTranslationListener(LayerView.DynamicToolbarListener aListener) {
        mListeners.remove(aListener);
    }

    private void fireListeners() {
        for (LayerView.DynamicToolbarListener listener : mListeners) {
            listener.onTranslationChanged(mToolbarTranslation, mLayerViewTranslation);
        }
    }

    void onPanZoomStopped() {
        for (LayerView.DynamicToolbarListener listener : mListeners) {
            listener.onPanZoomStopped();
        }
    }

    void onMetricsChanged(ImmutableViewportMetrics aMetrics) {
        for (LayerView.DynamicToolbarListener listener : mListeners) {
            listener.onMetricsChanged(aMetrics);
        }
    }

    public void setMaxTranslation(float maxTranslation) {
        ThreadUtils.assertOnUiThread();
        if (maxTranslation < 0) {
            Log.e(LOGTAG, "Got a negative max-translation value: " + maxTranslation + "; clamping to zero");
            mMaxTranslation = 0;
        } else {
            mMaxTranslation = maxTranslation;
        }
    }

    public float getMaxTranslation() {
        return mMaxTranslation;
    }

    public float getToolbarTranslation() {
        return mToolbarTranslation;
    }

    /**
     * If true, scroll changes will not affect translation.
     */
    public boolean isPinned() {
        return !pinFlags.isEmpty();
    }

    public boolean isPinnedBy(PinReason reason) {
        return pinFlags.contains(reason);
    }

    public void setPinned(boolean pinned, PinReason reason) {
        if (pinned) {
            pinFlags.add(reason);
        } else {
            pinFlags.remove(reason);
        }
    }

    public void showToolbar(boolean immediately) {
        animateToolbar(true, immediately);
    }

    public void hideToolbar(boolean immediately) {
        animateToolbar(false, immediately);
    }

    public void setScrollingRootContent(boolean isRootContent) {
        mScrollingRootContent = isRootContent;
    }

    private void animateToolbar(final boolean showToolbar, boolean immediately) {
        ThreadUtils.assertOnUiThread();

        if (mAnimationTask != null) {
            mTarget.getView().removeRenderTask(mAnimationTask);
            mAnimationTask = null;
        }

        float desiredTranslation = (showToolbar ? 0 : mMaxTranslation);
        Log.v(LOGTAG, "Requested " + (immediately ? "immediate " : "") + "toolbar animation to translation " + desiredTranslation);
        if (FloatUtils.fuzzyEquals(mToolbarTranslation, desiredTranslation)) {
            // If we're already pretty much in the desired position, don't bother
            // with a full animation; do an immediate jump
            immediately = true;
            Log.v(LOGTAG, "Changing animation to immediate jump");
        }

        if (showToolbar && immediately) {
            // Special case for showing the toolbar immediately: some of the call
            // sites expect this to happen synchronously, so let's do that. This
            // is safe because if we are showing the toolbar from a hidden state
            // there is no chance of showing garbage
            mToolbarTranslation = desiredTranslation;
            fireListeners();
            // And then proceed with the normal flow (some of which will be
            // a no-op now)...
        }

        if (!showToolbar) {
            // If we are hiding the toolbar, we need to move the LayerView first,
            // so that we don't end up showing garbage under the toolbar when
            // it is hidden. In the case that we are showing the toolbar, we
            // move the LayerView after the toolbar is shown - the
            // DynamicToolbarAnimationTask calls that upon completion.
            shiftLayerView(desiredTranslation);
        }

        mAnimationTask = new DynamicToolbarAnimationTask(desiredTranslation, immediately, showToolbar);
        mTarget.getView().postRenderTask(mAnimationTask);
    }

    private synchronized void shiftLayerView(float desiredTranslation) {
        float layerViewTranslationNeeded = desiredTranslation - mLayerViewTranslation;
        mLayerViewTranslation = desiredTranslation;
        synchronized (mTarget.getLock()) {
            mHeightDuringResize = new Integer(mTarget.getViewportMetrics().viewportRectHeight);
            mSnapRequired = mTarget.setViewportSize(
                mTarget.getView().getWidth(),
                mTarget.getView().getHeight() - Math.round(mMaxTranslation - mLayerViewTranslation),
                new PointF(0, -layerViewTranslationNeeded));
            if (!mSnapRequired) {
                mHeightDuringResize = null;
                ThreadUtils.postToUiThread(new Runnable() {
                    // Post to run it outside of the synchronize blocks. The
                    // delay shouldn't hurt.
                    @Override
                    public void run() {
                        fireListeners();
                    }
                });
            }
            // Request a composite, which will trigger the snap.
            mTarget.getView().requestRender();
        }
    }

    IntSize getViewportSize() {
        ThreadUtils.assertOnUiThread();

        int viewWidth = mTarget.getView().getWidth();
        int viewHeight = mTarget.getView().getHeight();
        float toolbarTranslation = mToolbarTranslation;
        if (mAnimationTask != null) {
            // If we have an animation going, mToolbarTranslation may be in flux
            // and we should use the final value it will settle on.
            toolbarTranslation = mAnimationTask.getFinalToolbarTranslation();
        }
        int viewHeightVisible = viewHeight - Math.round(mMaxTranslation - toolbarTranslation);
        return new IntSize(viewWidth, viewHeightVisible);
    }

    boolean isResizing() {
        return mHeightDuringResize != null;
    }

    private final Runnable mSnapRunnable = new Runnable() {
        private int mFrame = 0;

        @Override
        public final void run() {
            // It takes 2 frames for the view translation to take effect, at
            // least on a Nexus 4 device running Android 4.2.2. So we wait for
            // two frames before doing the notifyAll(), otherwise we get a
            // short user-visible glitch.
            // TODO: find a better way to do this, if possible.
            if (mFrame == 1) {
                synchronized (this) {
                    this.notifyAll();
                }
                mFrame = 0;
                return;
            }

            if (mFrame == 0) {
                fireListeners();
            }

            ViewCompat.postOnAnimation(mTarget.getView(), this);
            mFrame++;
        }
    };

    void scrollChangeResizeCompleted() {
        synchronized (mTarget.getLock()) {
            Log.v(LOGTAG, "Scrollchange resize completed");
            mHeightDuringResize = null;
        }
    }

    /**
     * "Shrinks" the absolute value of aValue by moving it closer to zero by
     * aShrinkAmount, but prevents it from crossing over zero. If aShrinkAmount
     * is negative it is ignored.
     * @return The shrunken value.
     */
    private static float shrinkAbs(float aValue, float aShrinkAmount) {
        if (aShrinkAmount <= 0) {
            return aValue;
        }
        float shrinkBy = Math.min(Math.abs(aValue), aShrinkAmount);
        return (aValue < 0 ? aValue + shrinkBy : aValue - shrinkBy);
    }

    /**
     * This function takes in a scroll amount and decides how much of that
     * should be used up to translate things on screen because of the dynamic
     * toolbar behaviour. It returns the maximum amount that could be used
     * for translation purposes; the rest must be used for scrolling.
     */
    private float decideTranslation(float aDelta,
                                    ImmutableViewportMetrics aMetrics,
                                    float aTouchTravelDistance) {

        float exposeThreshold = aMetrics.getHeight() * SCROLL_TOOLBAR_THRESHOLD;
        float translation = aDelta;

        if (translation < 0) { // finger moving upwards
            translation = shrinkAbs(translation, aMetrics.getOverscroll().top);

            // If the toolbar is in a state between fully hidden and fully shown
            // (i.e. the user is actively translating it), then we want the
            // translation to take effect right away. Or if the user has moved
            // their finger past the required threshold (and is not trying to
            // scroll past the bottom of the page) then also we want the touch
            // to cause translation. If the toolbar is fully visible, we only
            // want the toolbar to hide if the user is scrolling the root content.
            boolean inBetween = (mToolbarTranslation != 0 && mToolbarTranslation != mMaxTranslation);
            boolean reachedThreshold = -aTouchTravelDistance >= exposeThreshold;
            boolean atBottomOfPage = aMetrics.viewportRectBottom() >= aMetrics.pageRectBottom;
            if (inBetween || (mScrollingRootContent && reachedThreshold && !atBottomOfPage)) {
                return translation;
            }
        } else {    // finger moving downwards
            translation = shrinkAbs(translation, aMetrics.getOverscroll().bottom);

            // Ditto above comment, but in this case if they reached the top and
            // the toolbar is not shown, then we do want to allow translation
            // right away.
            boolean inBetween = (mToolbarTranslation != 0 && mToolbarTranslation != mMaxTranslation);
            boolean reachedThreshold = aTouchTravelDistance >= exposeThreshold;
            boolean atTopOfPage = aMetrics.viewportRectTop <= aMetrics.pageRectTop;
            boolean isToolbarTranslated = (mToolbarTranslation != 0);
            if (inBetween || reachedThreshold || (atTopOfPage && isToolbarTranslated)) {
                return translation;
            }
        }

        return 0;
    }

    // Timestamp of the start of the touch event used to calculate toolbar velocity
    private long mLastEventTime;
    // Current velocity of the toolbar. Used to populate the velocity queue in C++APZ.
    private float mVelocity;

    boolean onInterceptTouchEvent(MotionEvent event) {
        if (isPinned()) {
            return false;
        }

        // Animations should never co-exist with the user touching the screen.
        if (mAnimationTask != null) {
            mTarget.getView().removeRenderTask(mAnimationTask);
            mAnimationTask = null;
        }

        // we only care about single-finger drags here; any other kind of event
        // should reset and cause us to start over.
        if (event.getToolType(0) == MotionEvent.TOOL_TYPE_MOUSE ||
            event.getActionMasked() != MotionEvent.ACTION_MOVE ||
            event.getPointerCount() != 1)
        {
            if (mTouchStart != null) {
                Log.v(LOGTAG, "Resetting touch sequence due to non-move");
                mTouchStart = null;
                mVelocity = 0.0f;
            }

            if (event.getActionMasked() == MotionEvent.ACTION_UP) {
                // We need to do this even if the toolbar is already fully
                // visible or fully hidden, because this is what triggers the
                // viewport resize in content and updates the viewport metrics.
                boolean toolbarMostlyVisible = mToolbarTranslation < (mMaxTranslation / 2);
                Log.v(LOGTAG, "All fingers lifted, completing " + (toolbarMostlyVisible ? "show" : "hide"));
                animateToolbar(toolbarMostlyVisible, false);
            }
            return false;
        }

        if (mTouchStart != null) {
            float prevDir = mLastTouch - mTouchStart.y;
            float newDir = event.getRawY() - mLastTouch;
            if (prevDir != 0 && newDir != 0 && ((prevDir < 0) != (newDir < 0))) {
                // If the direction of movement changed, reset the travel
                // distance properties.
                mTouchStart = null;
                mVelocity = 0.0f;
            }
        }

        if (mTouchStart == null) {
            mTouchStart = new PointF(event.getRawX(), event.getRawY());
            mLastTouch = event.getRawY();
            mLastEventTime = event.getEventTime();
            return false;
        }

        float deltaY = event.getRawY() - mLastTouch;
        long currentTime = event.getEventTime();
        float deltaTime = (float)(currentTime - mLastEventTime);
        mLastEventTime = currentTime;
        if (deltaTime > 0.0f) {
            mVelocity = -deltaY / deltaTime;
        } else {
            mVelocity = 0.0f;
        }
        mLastTouch = event.getRawY();
        float travelDistance = event.getRawY() - mTouchStart.y;

        ImmutableViewportMetrics metrics = mTarget.getViewportMetrics();

        if (metrics.getPageHeight() <= mTarget.getView().getHeight() &&
            mToolbarTranslation == 0) {
            // If the page is short and the toolbar is already visible, don't
            // allow translating it out of view.
            return false;
        }

        float translation = decideTranslation(deltaY, metrics, travelDistance);

        float oldToolbarTranslation = mToolbarTranslation;
        float oldLayerViewTranslation = mLayerViewTranslation;
        mToolbarTranslation = FloatUtils.clamp(mToolbarTranslation - translation, 0, mMaxTranslation);
        mLayerViewTranslation = FloatUtils.clamp(mLayerViewTranslation - translation, 0, mMaxTranslation);

        if (oldToolbarTranslation == mToolbarTranslation &&
            oldLayerViewTranslation == mLayerViewTranslation) {
            return false;
        }

        if (mToolbarTranslation == mMaxTranslation) {
            Log.v(LOGTAG, "Toolbar at maximum translation, calling shiftLayerView(" + mMaxTranslation + ")");
            shiftLayerView(mMaxTranslation);
        } else if (mToolbarTranslation == 0) {
            Log.v(LOGTAG, "Toolbar at minimum translation, calling shiftLayerView(0)");
            shiftLayerView(0);
        }

        fireListeners();
        mTarget.getView().requestRender();
        return true;
    }

    // Get the current velocity of the toolbar.
    float getVelocity() {
        return mVelocity;
    }

    public PointF getVisibleEndOfLayerView() {
        return new PointF(mTarget.getView().getWidth(),
            mTarget.getView().getHeight() - mMaxTranslation + mLayerViewTranslation);
    }

    private float bottomOfCssViewport(ImmutableViewportMetrics aMetrics) {
        return (isResizing() ? mHeightDuringResize : aMetrics.getHeight())
                + mMaxTranslation - mLayerViewTranslation;
    }

    private synchronized boolean getAndClearSnapRequired() {
        boolean snapRequired = mSnapRequired;
        mSnapRequired = false;
        return snapRequired;
    }

    void populateViewTransform(ViewTransform aTransform, ImmutableViewportMetrics aMetrics) {
        if (getAndClearSnapRequired()) {
            synchronized (mSnapRunnable) {
                ViewCompat.postOnAnimation(mTarget.getView(), mSnapRunnable);
                try {
                    // hold the in-progress composite until the views have been
                    // translated because otherwise there is a visible glitch.
                    // don't hold for more than 100ms just in case.
                    mSnapRunnable.wait(100);
                } catch (InterruptedException ie) {
                }
            }
        }

        aTransform.x = aMetrics.viewportRectLeft;
        aTransform.y = aMetrics.viewportRectTop;
        aTransform.width = aMetrics.viewportRectWidth;
        aTransform.height = aMetrics.viewportRectHeight;
        aTransform.scale = aMetrics.zoomFactor;

        aTransform.fixedLayerMarginTop = mLayerViewTranslation - mToolbarTranslation;
        float bottomOfScreen = mTarget.getView().getHeight();
        // We want to move a fixed item from "bottomOfCssViewport" to
        // "bottomOfScreen". But also the bottom margin > 0 means that bottom
        // fixed-pos items will move upwards.
        aTransform.fixedLayerMarginBottom = bottomOfCssViewport(aMetrics) - bottomOfScreen;
        //Log.v(LOGTAG, "ViewTransform is x=" + aTransform.x + " y=" + aTransform.y
        //    + " z=" + aTransform.scale + " t=" + aTransform.fixedLayerMarginTop
        //    + " b=" + aTransform.fixedLayerMarginBottom);
    }

    class DynamicToolbarAnimationTask extends RenderTask {
        private final float mStartTranslation;
        private final float mEndTranslation;
        private final boolean mImmediate;
        private final boolean mShiftLayerView;
        private boolean mContinueAnimation;

        public DynamicToolbarAnimationTask(float aTranslation, boolean aImmediate, boolean aShiftLayerView) {
            super(false);
            mContinueAnimation = true;
            mStartTranslation = mToolbarTranslation;
            mEndTranslation = aTranslation;
            mImmediate = aImmediate;
            mShiftLayerView = aShiftLayerView;
        }

        float getFinalToolbarTranslation() {
            return mEndTranslation;
        }

        @Override
        public boolean internalRun(long timeDelta, long currentFrameStartTime) {
            if (!mContinueAnimation) {
                return false;
            }

            // Calculate the progress (between 0 and 1)
            final float progress = mImmediate
                ? 1.0f
                : mInterpolator.getInterpolation(
                    Math.min(1.0f, (System.nanoTime() - getStartTime())
                                    / (float)ANIMATION_DURATION));

            // This runs on the compositor thread, so we need to post the
            // actual work to the UI thread.
            ThreadUtils.assertNotOnUiThread();

            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    // Move the toolbar as per the animation
                    mToolbarTranslation = FloatUtils.interpolate(mStartTranslation, mEndTranslation, progress);
                    fireListeners();

                    if (mShiftLayerView && progress >= 1.0f) {
                        shiftLayerView(mEndTranslation);
                    }
                }
            });

            mTarget.getView().requestRender();
            if (progress >= 1.0f) {
                mContinueAnimation = false;
            }
            return mContinueAnimation;
        }
    }

    class SnapMetrics {
        public final int viewportWidth;
        public final int viewportHeight;
        public final float scrollChangeY;

        SnapMetrics(ImmutableViewportMetrics aMetrics, float aScrollChange) {
            viewportWidth = aMetrics.viewportRectWidth;
            viewportHeight = aMetrics.viewportRectHeight;
            scrollChangeY = aScrollChange;
        }
    }
}
