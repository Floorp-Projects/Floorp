/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import android.animation.Animator;
import android.animation.ValueAnimator;
import android.annotation.TargetApi;
import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.Handler;
import android.support.annotation.InterpolatorRes;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.view.ViewCompat;
import android.util.AttributeSet;
import android.view.animation.AnimationUtils;
import android.view.animation.Interpolator;
import android.view.animation.LinearInterpolator;

import org.mozilla.gecko.DynamicToolbar;
import org.mozilla.gecko.R;
import org.mozilla.gecko.drawable.ShiftDrawable;
import org.mozilla.gecko.gfx.DynamicToolbarAnimator;
import org.mozilla.gecko.widget.themed.ThemedProgressBar;

/**
 * A progressbar with some animations on changing progress.
 * When changing progress of this bar, it does not change value directly. Instead, it use
 * {@link Animator} to change value progressively. Moreover, change visibility to View.GONE will
 * cause closing animation.
 */
public class AnimatedProgressBar extends ThemedProgressBar {

    /**
     * Animation duration of progress changing.
     */
    private final static int PROGRESS_DURATION = 200;

    /**
     * Delay before applying closing animation when progress reach max value.
     */
    private final static int CLOSING_DELAY = 300;

    /**
     * Animation duration for closing
     */
    private final static int CLOSING_DURATION = 300;

    private ValueAnimator mPrimaryAnimator;
    private final ValueAnimator mClosingAnimator = ValueAnimator.ofFloat(0f, 1f);

    /**
     * For closing animation. To indicate how many visible region should be clipped.
     */
    private float mClipRatio = 0f;
    private final Rect mRect = new Rect();

    /**
     * To store the final expected progress to reach, it does matter in animation.
     */
    private int mExpectedProgress = 0;

    /**
     * setProgress() might be invoked in constructor. Add to flag to avoid null checking for animators.
     */
    private boolean mInitialized = false;

    private boolean mIsRtl = false;

    private DynamicToolbar mDynamicToolbar;
    private EndingRunner mEndingRunner = new EndingRunner();

    private final ValueAnimator.AnimatorUpdateListener mListener =
            new ValueAnimator.AnimatorUpdateListener() {
                @Override
                public void onAnimationUpdate(ValueAnimator animation) {
                    setProgressImmediately((int) mPrimaryAnimator.getAnimatedValue());
                }
            };

    public AnimatedProgressBar(@NonNull Context context) {
        super(context, null);
        init(context, null);
    }

    public AnimatedProgressBar(@NonNull Context context,
                               @Nullable AttributeSet attrs) {
        super(context, attrs);
        init(context, attrs);
    }

    public AnimatedProgressBar(@NonNull Context context,
                               @Nullable AttributeSet attrs,
                               int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init(context, attrs);
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    public AnimatedProgressBar(Context context,
                               AttributeSet attrs,
                               int defStyleAttr,
                               int defStyleRes) {
        super(context, attrs, defStyleAttr);
        init(context, attrs);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public synchronized void setMax(int max) {
        super.setMax(max);
        mPrimaryAnimator = createAnimator(getMax(), mListener);
    }

    /**
     * {@inheritDoc}
     * <p>
     * Instead of set progress directly, this method triggers an animator to change progress.
     */
    @Override
    public void setProgress(int nextProgress) {
        nextProgress = Math.min(nextProgress, getMax());
        nextProgress = Math.max(0, nextProgress);
        mExpectedProgress = nextProgress;
        if (!mInitialized) {
            setProgressImmediately(mExpectedProgress);
            return;
        }

        // if regress, jump to the expected value without any animation
        if (mExpectedProgress < getProgress()) {
            cancelAnimations();
            setProgressImmediately(mExpectedProgress);
            return;
        }

        // Animation is not needed for reloading a completed page
        if ((mExpectedProgress == 0) && (getProgress() == getMax())) {
            cancelAnimations();
            setProgressImmediately(0);
            return;
        }

        cancelAnimations();
        mPrimaryAnimator.setIntValues(getProgress(), nextProgress);
        mPrimaryAnimator.start();
    }

    @Override
    public void onDraw(Canvas canvas) {
        if (mClipRatio == 0) {
            super.onDraw(canvas);
        } else {
            canvas.getClipBounds(mRect);
            final float clipWidth = mRect.width() * mClipRatio;
            canvas.save();
            if (mIsRtl) {
                canvas.clipRect(mRect.left, mRect.top, mRect.right - clipWidth, mRect.bottom);
            } else {
                canvas.clipRect(mRect.left + clipWidth, mRect.top, mRect.right, mRect.bottom);
            }
            super.onDraw(canvas);
            canvas.restore();
        }
    }

    /**
     * {@inheritDoc}
     * <p>
     * Instead of change visibility directly, this method also applies the closing animation if
     * progress reaches max value.
     */
    @Override
    public void setVisibility(int value) {
        // nothing changed
        if (getVisibility() == value) {
            return;
        }

        if (value == GONE) {
            if (mExpectedProgress == getMax()) {
                setProgressImmediately(mExpectedProgress);
                animateClosing();
            } else {
                setVisibilityImmediately(value);
            }
        } else {
            final Handler handler = getHandler();
            // if this view is detached from window, the handler would be null
            if (handler != null) {
                handler.removeCallbacks(mEndingRunner);
            }

            if (mClosingAnimator != null) {
                mClipRatio = 0;
                mClosingAnimator.cancel();
            }
            setVisibilityImmediately(value);
        }
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();
        mIsRtl = (ViewCompat.getLayoutDirection(this) == ViewCompat.LAYOUT_DIRECTION_RTL);
    }

    public void setDynamicToolbar(@Nullable DynamicToolbar toolbar) {
        mDynamicToolbar = toolbar;
    }

    public void pinDynamicToolbar() {
        if (mDynamicToolbar == null) {
            return;
        }
        if (mDynamicToolbar.isEnabled()) {
            mDynamicToolbar.setPinned(true, DynamicToolbarAnimator.PinReason.PAGE_LOADING);
            mDynamicToolbar.setVisible(true, DynamicToolbar.VisibilityTransition.ANIMATE);
        }
    }

    public void unpinDynamicToolbar() {
        if (mDynamicToolbar == null) {
            return;
        }
        if (mDynamicToolbar.isEnabled()) {
            mDynamicToolbar.setPinned(false, DynamicToolbarAnimator.PinReason.PAGE_LOADING);
        }
    }

    private void cancelAnimations() {
        if (mPrimaryAnimator != null) {
            mPrimaryAnimator.cancel();
        }
        if (mClosingAnimator != null) {
            mClosingAnimator.cancel();
        }

        mClipRatio = 0;
    }

    private void init(@NonNull Context context, @Nullable AttributeSet attrs) {
        mInitialized = true;

        final TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.AnimatedProgressBar);
        final int duration = a.getInteger(R.styleable.AnimatedProgressBar_shiftDuration, 1000);
        final boolean wrap = a.getBoolean(R.styleable.AnimatedProgressBar_wrapShiftDrawable, false);
        @InterpolatorRes final int itplId = a.getResourceId(R.styleable.AnimatedProgressBar_shiftInterpolator, 0);
        a.recycle();

        setProgressDrawable(buildDrawable(getProgressDrawable(), wrap, duration, itplId));

        mPrimaryAnimator = createAnimator(getMax(), mListener);

        mClosingAnimator.setDuration(CLOSING_DURATION);
        mClosingAnimator.setInterpolator(new LinearInterpolator());
        mClosingAnimator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
            @Override
            public void onAnimationUpdate(ValueAnimator valueAnimator) {
                mClipRatio = (float) valueAnimator.getAnimatedValue();
                invalidate();
            }
        });
        mClosingAnimator.addListener(new Animator.AnimatorListener() {
            @Override
            public void onAnimationStart(Animator animator) {
                mClipRatio = 0f;
            }

            @Override
            public void onAnimationEnd(Animator animator) {
                setVisibilityImmediately(GONE);
            }

            @Override
            public void onAnimationCancel(Animator animator) {
                mClipRatio = 0f;
            }

            @Override
            public void onAnimationRepeat(Animator animator) {
            }
        });
    }

    private void setVisibilityImmediately(int value) {
        super.setVisibility(value);
    }

    private void animateClosing() {
        mClosingAnimator.cancel();
        final Handler handler = getHandler();
        // if this view is detached from window, the handler would be null
        if (handler != null) {
            handler.removeCallbacks(mEndingRunner);
            handler.postDelayed(mEndingRunner, CLOSING_DELAY);
        }
    }

    private void setProgressImmediately(int progress) {
        super.setProgress(progress);
    }

    private Drawable buildDrawable(@NonNull Drawable original,
                                   boolean isWrap,
                                   int duration,
                                   @InterpolatorRes int itplId) {
        if (isWrap) {
            final Interpolator interpolator = (itplId > 0)
                    ? AnimationUtils.loadInterpolator(getContext(), itplId)
                    : null;
            return new ShiftDrawable(original, duration, interpolator);
        } else {
            return original;
        }
    }

    private static ValueAnimator createAnimator(int max, ValueAnimator.AnimatorUpdateListener listener) {
        ValueAnimator animator = ValueAnimator.ofInt(0, max);
        animator.setInterpolator(new LinearInterpolator());
        animator.setDuration(PROGRESS_DURATION);
        animator.addUpdateListener(listener);
        return animator;
    }

    private class EndingRunner implements Runnable {
        @Override
        public void run() {
            mClosingAnimator.start();
        }
    }
}
