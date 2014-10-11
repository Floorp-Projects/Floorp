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
        final boolean needsAnimationProxy = Versions.preHC;

        // If the view's animation proxy has been overridden from somewhere else, we need to
        // create a new AnimatorProxy for the view.
        if (proxy == null || (needsAnimationProxy && proxy.mImpl != view.getAnimation())) {
            AnimatorProxyImpl impl = (needsAnimationProxy ? new AnimatorProxyPreHC(view) :
                                                            new AnimatorProxyPostHC(view));

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

    /*
     * AnimatorProxyPreHC uses the technique used by the NineOldAndroids described here:
     * http://jakewharton.com/advanced-pre-honeycomb-animation/
     *
     * Some of this code is based on Jake Wharton's AnimatorProxy released as part of
     * the NineOldAndroids library under the Apache License 2.0.
     */
    private static class AnimatorProxyPreHC extends Animation implements AnimatorProxyImpl {
        private final WeakReference<View> mViewRef;

        private final RectF mBefore;
        private final RectF mAfter;
        private final Matrix mTempMatrix;

        private float mAlpha;
        private float mTranslationX;
        private float mTranslationY;

        public AnimatorProxyPreHC(View view) {
            mBefore = new RectF();
            mAfter = new RectF();
            mTempMatrix = new Matrix();

            mAlpha = 1;

            loadCurrentTransformation(view);

            setDuration(0);
            setFillAfter(true);
            view.setAnimation(this);

            mViewRef = new WeakReference<View>(view);
        }

        private void loadCurrentTransformation(View view) {
            Animation animation = view.getAnimation();
            if (animation == null)
                return;

            Transformation transformation = new Transformation();
            float[] matrix = new float[9];

            animation.getTransformation(AnimationUtils.currentAnimationTimeMillis(), transformation);
            transformation.getMatrix().getValues(matrix);

            mAlpha = transformation.getAlpha();
            mTranslationX = matrix[Matrix.MTRANS_X];
            mTranslationY = matrix[Matrix.MTRANS_Y];
        }

        private void prepareForUpdate() {
            View view = mViewRef.get();
            if (view != null)
                computeRect(mBefore, view);
        }

        private void computeRect(final RectF r, View view) {
            final float w = view.getWidth();
            final float h = view.getHeight();

            r.set(0, 0, w, h);

            final Matrix m = mTempMatrix;
            m.reset();
            transformMatrix(m, view);
            mTempMatrix.mapRect(r);

            r.offset(view.getLeft(), view.getTop());
        }

        private void transformMatrix(Matrix m, View view) {
            m.postTranslate(mTranslationX, mTranslationY);
        }

        private void invalidateAfterUpdate() {
            View view = mViewRef.get();
            if (view == null || view.getParent() == null)
                return;

            final RectF after = mAfter;
            computeRect(after, view);
            after.union(mBefore);

            ((View)view.getParent()).invalidate(
                    (int) Math.floor(after.left),
                    (int) Math.floor(after.top),
                    (int) Math.ceil(after.right),
                    (int) Math.ceil(after.bottom));
        }

        @Override
        public float getAlpha() {
            return mAlpha;
        }

        @Override
        public void setAlpha(float alpha) {
            if (mAlpha == alpha)
                return;

            mAlpha = alpha;

            View view = mViewRef.get();
            if (view != null)
                view.invalidate();
        }

        @Override
        public float getTranslationX() {
            return mTranslationX;
        }

        @Override
        public void setTranslationX(float translationX) {
            if (mTranslationX == translationX)
                return;

            prepareForUpdate();
            mTranslationX = translationX;
            invalidateAfterUpdate();
        }

        @Override
        public float getTranslationY() {
            return mTranslationY;
        }

        @Override
        public void setTranslationY(float translationY) {
            if (mTranslationY == translationY)
                return;

            prepareForUpdate();
            mTranslationY = translationY;
            invalidateAfterUpdate();
        }

        @Override
        public View getView() {
            return mViewRef.get();
        }

        @Override
        protected void applyTransformation(float interpolatedTime, Transformation t) {
            View view = mViewRef.get();
            if (view != null) {
                t.setAlpha(mAlpha);
                transformMatrix(t.getMatrix(), view);
            }
        }
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

