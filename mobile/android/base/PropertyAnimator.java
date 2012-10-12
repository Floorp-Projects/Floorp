/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.os.Build;
import android.os.Handler;
import android.util.Log;
import android.view.Choreographer;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.AnimationUtils;
import android.view.animation.DecelerateInterpolator;
import android.view.animation.Interpolator;

import java.util.ArrayList;
import java.util.List;
import java.util.Timer;
import java.util.TimerTask;

public class PropertyAnimator implements Runnable {
    private static final String LOGTAG = "GeckoPropertyAnimator";

    public static enum Property {
        SHRINK_LEFT,
        SHRINK_TOP,
        SLIDE_TOP,
        SLIDE_LEFT
    }

    private class ElementHolder {
        View view;
        Property property;
        int from;
        int to;
    }

    public static interface PropertyAnimationListener {
        public void onPropertyAnimationStart();
        public void onPropertyAnimationEnd();
    }

    private Interpolator mInterpolator;
    private long mStartTime;
    private long mDuration;
    private float mDurationReciprocal;
    private List<ElementHolder> mElementsList;
    private PropertyAnimationListener mListener;
    private FramePoster mFramePoster;

    public PropertyAnimator(int duration) {
        this(duration, new DecelerateInterpolator());
    }

    public PropertyAnimator(int duration, Interpolator interpolator) {
        mDuration = duration;
        mDurationReciprocal = 1.0f / (float) mDuration;
        mInterpolator = interpolator;
        mElementsList = new ArrayList<ElementHolder>();
        mFramePoster = FramePoster.create(this);
    }

    public void attach(View view, Property property, int to) {
        if (!(view instanceof ViewGroup) && (property == Property.SHRINK_LEFT ||
                                             property == Property.SHRINK_TOP)) {
            Log.i(LOGTAG, "Margin can only be animated on Viewgroups");
            return;
        }

        ElementHolder element = new ElementHolder();

        element.view = view;
        element.property = property;

        // Sliding should happen in the negative.
        if (property == Property.SLIDE_TOP || property == Property.SLIDE_LEFT)
            element.to = to * -1;
        else
            element.to = to;

        mElementsList.add(element);
    }

    public void setPropertyAnimationListener(PropertyAnimationListener listener) {
        mListener = listener;
    }

    @Override
    public void run() {
        int timePassed = (int) (AnimationUtils.currentAnimationTimeMillis() - mStartTime);
        if (timePassed >= mDuration) {
            stop();
            return;
        }

        float interpolation = mInterpolator.getInterpolation(timePassed * mDurationReciprocal);

        for (ElementHolder element : mElementsList) { 
            int delta = element.from + (int) ((element.to - element.from) * interpolation);
            invalidate(element, delta);
        }

        mFramePoster.postNextAnimationFrame();
    }

    public void start() {
        mStartTime = AnimationUtils.currentAnimationTimeMillis();

        // Fix the from value based on current position and property
        for (ElementHolder element : mElementsList) {
            if (element.property == Property.SLIDE_TOP)
                element.from = element.view.getScrollY();
            else if (element.property == Property.SLIDE_LEFT)
                element.from = element.view.getScrollX();
            else {
                ViewGroup.MarginLayoutParams params = ((ViewGroup.MarginLayoutParams) element.view.getLayoutParams());
                if (element.property == Property.SHRINK_TOP)
                    element.from = params.topMargin;
                else if (element.property == Property.SHRINK_LEFT)
                    element.from = params.leftMargin;
            }
        }

        if (mDuration != 0) {
            mFramePoster.postFirstAnimationFrame();

            if (mListener != null)
                mListener.onPropertyAnimationStart();
        }
    }

    public void stop() {
        mFramePoster.cancelAnimationFrame();

        // Make sure to snap to the end position.
        for (ElementHolder element : mElementsList) { 
            invalidate(element, element.to);
        }

        mElementsList.clear();

        if (mListener != null) {
            mListener.onPropertyAnimationEnd();
            mListener = null;
        }
    }

    private void invalidate(final ElementHolder element, final int delta) {
        final View view = element.view;

        // check to see if the view was detached between the check above and this code
        // getting run on the UI thread.
        if (view.getHandler() == null)
            return;

        if (element.property == Property.SLIDE_TOP) {
            view.scrollTo(view.getScrollX(), delta);
            return;
        } else if (element.property == Property.SLIDE_LEFT) {
            view.scrollTo(delta, view.getScrollY());
            return;
        }

        ViewGroup.MarginLayoutParams params = (ViewGroup.MarginLayoutParams) view.getLayoutParams();

        if (element.property == Property.SHRINK_TOP)
            params.setMargins(params.leftMargin, delta, params.rightMargin, params.bottomMargin);
        else if (element.property == Property.SHRINK_LEFT)
            params.setMargins(delta, params.topMargin, params.rightMargin, params.bottomMargin);

        view.requestLayout();
    }

    private static abstract class FramePoster {
        public static FramePoster create(Runnable r) {
            if (Build.VERSION.SDK_INT >= 16)
                return new FramePosterPostJB(r);
            else
                return new FramePosterPreJB(r);
        }

        public abstract void postFirstAnimationFrame();
        public abstract void postNextAnimationFrame();
        public abstract void cancelAnimationFrame();
    }

    private static class FramePosterPreJB extends FramePoster {
        // Default refresh rate in ms.
        private static final int INTERVAL = 10;

        private Handler mHandler;
        private Runnable mRunnable;

        public FramePosterPreJB(Runnable r) {
            mHandler = new Handler();
            mRunnable = r;
        }

        @Override
        public void postFirstAnimationFrame() {
            mHandler.post(mRunnable);
        }

        @Override
        public void postNextAnimationFrame() {
            mHandler.postDelayed(mRunnable, INTERVAL);
        }

        @Override
        public void cancelAnimationFrame() {
            mHandler.removeCallbacks(mRunnable);
        }
    }

    private static class FramePosterPostJB extends FramePoster {
        private Choreographer mChoreographer;
        private Choreographer.FrameCallback mCallback;

        public FramePosterPostJB(final Runnable r) {
            mChoreographer = Choreographer.getInstance();

            mCallback = new Choreographer.FrameCallback() {
                @Override
                public void doFrame(long frameTimeNanos) {
                    r.run();
                }
            };
        }

        @Override
        public void postFirstAnimationFrame() {
            postNextAnimationFrame();
        }

        @Override
        public void postNextAnimationFrame() {
            mChoreographer.postFrameCallback(mCallback);
        }

        @Override
        public void cancelAnimationFrame() {
            mChoreographer.removeFrameCallback(mCallback);
        }
    }
}
