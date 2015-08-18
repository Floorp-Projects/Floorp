/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.PrefsHelper;
import org.mozilla.gecko.util.FloatUtils;
import org.mozilla.gecko.util.ThreadUtils;

import android.util.Log;
import android.view.animation.DecelerateInterpolator;

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
