/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.PrefsHelper;
import org.mozilla.gecko.util.FloatUtils;
import org.mozilla.gecko.util.ThreadUtils;

import android.graphics.PointF;
import android.util.Log;
import android.view.animation.DecelerateInterpolator;
import android.view.MotionEvent;

import java.util.ArrayList;
import java.util.List;

public class DynamicToolbarAnimator {
    private static final String LOGTAG = "GeckoDynamicToolbarAnimator";
    private static final String PREF_SCROLL_TOOLBAR_THRESHOLD = "browser.ui.scroll-toolbar-threshold";

    // The duration of the animation in ns
    private static final long ANIMATION_DURATION = 250000000;

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

    /* If this boolean is true, scroll changes will not affect translation */
    private boolean mPinned;

    /* This interpolator is used for the above mentioned animation */
    private DecelerateInterpolator mInterpolator;

    /* This is the proportion of the viewport rect that needs to be travelled
     * while scrolling before the translation will start taking effect.
     */
    private float SCROLL_TOOLBAR_THRESHOLD = 0.20f;
    /* The ID of the prefs listener for the scroll-toolbar threshold */
    private Integer mPrefObserverId;

    /* The task that handles showing/hiding toolbar */
    private DynamicToolbarAnimationTask mAnimationTask;

    /* The start point of a drag, used for scroll-based dynamic toolbar
     * behaviour. */
    private PointF mTouchStart;
    private float mLastTouch;

    public DynamicToolbarAnimator(GeckoLayerClient aTarget) {
        mTarget = aTarget;
        mListeners = new ArrayList<LayerView.DynamicToolbarListener>();

        mInterpolator = new DecelerateInterpolator();

        // Listen to the dynamic toolbar pref
        mPrefObserverId = PrefsHelper.getPref(PREF_SCROLL_TOOLBAR_THRESHOLD, new PrefsHelper.PrefHandlerBase() {
            @Override
            public void prefValue(String pref, int value) {
                SCROLL_TOOLBAR_THRESHOLD = value / 100.0f;
            }

            @Override
            public boolean isObserver() {
                return true;
            }
        });
    }

    public void destroy() {
        if (mPrefObserverId != null) {
            PrefsHelper.removeObserver(mPrefObserverId);
            mPrefObserverId = null;
        }
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

    public void setMaxTranslation(float maxTranslation) {
        ThreadUtils.assertOnUiThread();
        if (maxTranslation < 0) {
            Log.e(LOGTAG, "Got a negative max-translation value: " + maxTranslation + "; clamping to zero");
            mMaxTranslation = 0;
        } else {
            mMaxTranslation = maxTranslation;
        }
    }

    public void setPinned(boolean pinned) {
        mPinned = pinned;
    }

    public boolean isPinned() {
        return mPinned;
    }

    public void showToolbar(boolean immediately) {
        animateToolbar(0, immediately);
    }

    public void hideToolbar(boolean immediately) {
        animateToolbar(mMaxTranslation, immediately);
    }

    private void animateToolbar(final float translation, boolean immediately) {
        ThreadUtils.assertOnUiThread();

        if (mAnimationTask != null) {
            mTarget.getView().removeRenderTask(mAnimationTask);
            mAnimationTask = null;
        }

        Log.v(LOGTAG, "Requested " + (immediately ? "immedate " : "") + "toolbar animation to translation " + translation);
        if (FloatUtils.fuzzyEquals(mToolbarTranslation, translation)) {
            // If we're already pretty much in the desired position, don't bother
            // with a full animation; do an immediate jump
            immediately = true;
            Log.v(LOGTAG, "Changing animation to immediate jump");
        }

        if (immediately) {
            mToolbarTranslation = translation;
            fireListeners();
            resizeViewport();
            mTarget.getView().requestRender();
            return;
        }

        Log.v(LOGTAG, "Kicking off animation...");
        mAnimationTask = new DynamicToolbarAnimationTask(false, translation);
        mTarget.getView().postRenderTask(mAnimationTask);
    }

    private void resizeViewport() {
        ThreadUtils.assertOnUiThread();

        // The animation is done, now we need to tell gecko to resize to the
        // proper steady-state layout.
        synchronized (mTarget.getLock()) {
            int viewWidth = mTarget.getView().getWidth();
            int viewHeight = mTarget.getView().getHeight();
            int viewHeightVisible = viewHeight - Math.round(mMaxTranslation - mToolbarTranslation);

            Log.v(LOGTAG, "Resize viewport to dimensions " + viewWidth + "x" + viewHeightVisible);
            mTarget.setViewportSize(viewWidth, viewHeightVisible);
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
            // to cause translation.
            boolean inBetween = (mToolbarTranslation != 0 && mToolbarTranslation != mMaxTranslation);
            boolean reachedThreshold = -aTouchTravelDistance >= exposeThreshold;
            boolean atBottomOfPage = aMetrics.viewportRectBottom() >= aMetrics.pageRectBottom;
            if (inBetween || (reachedThreshold && !atBottomOfPage)) {
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

    boolean onInterceptTouchEvent(MotionEvent event) {
        if (mPinned) {
            return false;
        }

        // Animations should never co-exist with the user touching the screen.
        if (mAnimationTask != null) {
            mTarget.getView().removeRenderTask(mAnimationTask);
            mAnimationTask = null;
        }

        // we only care about single-finger drags here; any other kind of event
        // should reset and cause us to start over.
        if (event.getActionMasked() != MotionEvent.ACTION_MOVE ||
            event.getPointerCount() != 1)
        {
            if (mTouchStart != null) {
                Log.v(LOGTAG, "Resetting touch sequence due to non-move");
                mTouchStart = null;
            }
            return false;
        }

        if (mTouchStart != null) {
            float prevDir = mLastTouch - mTouchStart.y;
            float newDir = event.getRawY() - mLastTouch;
            if (prevDir != 0 && newDir != 0 && ((prevDir < 0) != (newDir < 0))) {
                Log.v(LOGTAG, "Direction changed: " + mTouchStart.y + " -> " + mLastTouch + " -> " + event.getRawY());
                // If the direction of movement changed, reset the travel
                // distance properties.
                mTouchStart = null;
            }
        }

        if (mTouchStart == null) {
            mTouchStart = new PointF(event.getRawX(), event.getRawY());
            mLastTouch = event.getRawY();
            return false;
        }

        float deltaY = event.getRawY() - mLastTouch;
        mLastTouch = event.getRawY();
        float travelDistance = event.getRawY() - mTouchStart.y;

        ImmutableViewportMetrics metrics = mTarget.getViewportMetrics();

        if (metrics.getPageHeight() < metrics.getHeight()) {
            return false;
        }

        float translation = decideTranslation(deltaY, metrics, travelDistance);
        Log.v(LOGTAG, "Got vertical translation " + translation);

        float oldToolbarTranslation = mToolbarTranslation;
        float oldLayerViewTranslation = mLayerViewTranslation;
        mToolbarTranslation = FloatUtils.clamp(mToolbarTranslation - translation, 0, mMaxTranslation);
        mLayerViewTranslation = FloatUtils.clamp(mLayerViewTranslation - translation, 0, mMaxTranslation);

        if (oldToolbarTranslation == mToolbarTranslation &&
            oldLayerViewTranslation == mLayerViewTranslation) {
            return false;
        }

        fireListeners();
        mTarget.getView().requestRender();
        return true;
    }

    class DynamicToolbarAnimationTask extends RenderTask {
        private final float mStartTranslation;
        private final float mEndTranslation;
        private boolean mContinueAnimation;

        public DynamicToolbarAnimationTask(boolean aRunAfter, float aTranslation) {
            super(aRunAfter);
            mContinueAnimation = true;
            mStartTranslation = mToolbarTranslation;
            mEndTranslation = aTranslation;
        }

        @Override
        public boolean internalRun(long timeDelta, long currentFrameStartTime) {
            if (!mContinueAnimation) {
                return false;
            }

            // Calculate the progress (between 0 and 1)
            final float progress = mInterpolator.getInterpolation(
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
                    // Move the LayerView so that there is never a gap between the toolbar
                    // and the LayerView, because there we will draw garbage.
                    mLayerViewTranslation = Math.max(mLayerViewTranslation, mToolbarTranslation);
                    fireListeners();

                    if (progress >= 1.0f) {
                        resizeViewport();
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
}
