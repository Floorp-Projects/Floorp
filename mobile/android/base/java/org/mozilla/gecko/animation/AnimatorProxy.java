/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.animation;

import java.lang.ref.WeakReference;
import java.util.WeakHashMap;

import org.mozilla.gecko.AppConstants.Versions;

import android.graphics.Matrix;
import android.graphics.RectF;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.Animation;
import android.view.animation.AnimationUtils;
import android.view.animation.Transformation;

class AnimatorProxy {
    private static final WeakHashMap<View, AnimatorProxy> PROXIES =
            new WeakHashMap<View, AnimatorProxy>();

    private static interface AnimatorProxyImpl {
        public float getAlpha();
        public void setAlpha(float alpha);

        public float getTranslationX();
        public void setTranslationX(float translationX);

        public float getTranslationY();
        public void setTranslationY(float translationY);

        public View getView();
    }

    private final AnimatorProxyImpl mImpl;

    private AnimatorProxy(AnimatorProxyImpl impl) {
        mImpl = impl;
    }

    public static AnimatorProxy create(View view) {
        AnimatorProxy proxy = PROXIES.get(view);
        // If the view's animation proxy has been overridden from somewhere else, we need to
        // create a new AnimatorProxy for the view.
        if (proxy == null) {
            AnimatorProxyImpl impl = (new AnimatorProxyPostHC(view));

            proxy = new AnimatorProxy(impl);
            PROXIES.put(view, proxy);
        }

        return proxy;
    }

    public int getWidth() {
        View view = mImpl.getView();
        if (view != null)
            return view.getWidth();

        return 0;
    }

    public void setWidth(int width) {
        View view = mImpl.getView();
        if (view != null) {
            ViewGroup.LayoutParams lp = view.getLayoutParams();
            lp.width = width;
            view.setLayoutParams(lp);
        }
    }

    public int getHeight() {
        View view = mImpl.getView();
        if (view != null)
            return view.getHeight();

        return 0;
    }

    public void setHeight(int height) {
        View view = mImpl.getView();
        if (view != null) {
            ViewGroup.LayoutParams lp = view.getLayoutParams();
            lp.height = height;
            view.setLayoutParams(lp);
        }
    }

    public int getScrollX() {
        View view = mImpl.getView();
        if (view != null)
            return view.getScrollX();

        return 0;
    }

    public int getScrollY() {
        View view = mImpl.getView();
        if (view != null)
            return view.getScrollY();

        return 0;
    }

    public void scrollTo(int scrollX, int scrollY) {
        View view = mImpl.getView();
        if (view != null)
            view.scrollTo(scrollX, scrollY);
    }

    public float getAlpha() {
        return mImpl.getAlpha();
    }

    public void setAlpha(float alpha) {
        mImpl.setAlpha(alpha);
    }

    public float getTranslationX() {
        return mImpl.getTranslationX();
    }

    public void setTranslationX(float translationX) {
        mImpl.setTranslationX(translationX);
    }

    public float getTranslationY() {
        return mImpl.getTranslationY();
    }

    public void setTranslationY(float translationY) {
        mImpl.setTranslationY(translationY);
    }

    private static class AnimatorProxyPostHC implements AnimatorProxyImpl {
        private final WeakReference<View> mViewRef;

        public AnimatorProxyPostHC(View view) {
            mViewRef = new WeakReference<View>(view);
        }

        @Override
        public float getAlpha() {
            View view = mViewRef.get();
            if (view != null)
                return view.getAlpha();

            return 1;
        }

        @Override
        public void setAlpha(float alpha) {
            View view = mViewRef.get();
            if (view != null)
                view.setAlpha(alpha);
        }

        @Override
        public float getTranslationX() {
            View view = mViewRef.get();
            if (view != null)
                return view.getTranslationX();

            return 0;
        }

        @Override
        public void setTranslationX(float translationX) {
            View view = mViewRef.get();
            if (view != null)
                view.setTranslationX(translationX);
        }

        @Override
        public float getTranslationY() {
            View view = mViewRef.get();
            if (view != null)
                return view.getTranslationY();

            return 0;
        }

        @Override
        public void setTranslationY(float translationY) {
            View view = mViewRef.get();
            if (view != null)
                view.setTranslationY(translationY);
        }

        @Override
        public View getView() {
            return mViewRef.get();
        }
    }
}

