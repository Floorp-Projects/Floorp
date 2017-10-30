/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.LinearGradient;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.graphics.Shader;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.TouchDelegate;
import android.view.View;
import android.widget.HorizontalScrollView;

import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.R;

/**
 * A {@link HorizontalScrollView} implementation with a more efficient fadingEdge drawing strategy
 * than the built-in version provided by Android. The width of the fade effect can be controlled via
 * <code>gecko:fadeWidth</code>. To control in how far the fading effect should affect any views
 * further up in the View hierarchy, place this view or one of its parents onto a separate layer
 * using <code>android:layerType</code>. Currently, only horizontal fading is supported.
 * <p>
 * Additionally, {@link TouchDelegate} support (which isn't provided for in Android's ScrollView
 * implementation) has been enabled.
 */
public class FadedHorizontalScrollView extends HorizontalScrollView {
    // Width of the fade effect from end of the view.
    private final int mFadeWidth;
    private final boolean mPreMarshmallow;

    private final FadePaint mFadePaint;
    private float mFadeTop;
    private float mFadeBottom;
    private boolean mVerticalFadeBordersDirty;

    private boolean mInterceptingTouchEvents;

    public FadedHorizontalScrollView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mPreMarshmallow = Versions.preMarshmallow;

        mFadePaint = new FadePaint();
        mVerticalFadeBordersDirty = true;
        addOnLayoutChangeListener(new OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom, int oldLeft, int oldTop, int oldRight, int oldBottom) {
                final int oldHeight = oldBottom - oldTop;
                if (getHeight() != oldHeight) {
                    mVerticalFadeBordersDirty = true;
                }
            }
        });

        final TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.FadedTextView);
        mFadeWidth = a.getDimensionPixelSize(R.styleable.FadedTextView_fadeWidth, 0);
        a.recycle();
    }

    @Override
    public int getHorizontalFadingEdgeLength() {
        return mFadeWidth;
    }

    @Override
    public void draw(Canvas canvas) {
        super.draw(canvas);
        if (mPreMarshmallow) {
            // Let our descendants draw their contents first, so we can then fade them out.
            drawFading(canvas);
        }
    }

    @TargetApi(23)
    @Override
    public void onDrawForeground(Canvas canvas) {
        // Our descendants have already painted, so we can draw the fading first to avoid fading out
        // any scrollbars etc. as well.
        drawFading(canvas);
        super.onDrawForeground(canvas);
    }

    private void drawFading(final Canvas canvas) {
        // This code here is mostly a condensed version of Android's fadingEdge implementation
        // in View#draw.

        final int left = getScrollX() + getPaddingLeft();
        final int right = getScrollX() + getRight() - getLeft() - getPaddingRight();

        // Clip the fade length to prevent the opposing fadingEdges from overlapping each other.
        int fadeWidth = getHorizontalFadingEdgeLength();
        if (left + fadeWidth > right - fadeWidth) {
            fadeWidth = (right - left) / 2;
        }

        final float effectiveFadeLeft = fadeWidth * getLeftFadingEdgeStrength();
        final float effectiveFadeRight = fadeWidth * getRightFadingEdgeStrength();
        final boolean drawLeft = effectiveFadeLeft > 1.0f;
        final boolean drawRight = effectiveFadeRight > 1.0f;

        if (!drawLeft && !drawRight) {
            return;
        }

        if (mVerticalFadeBordersDirty) {
            updateVerticalFadeBorders();
        }

        final Matrix matrix = mFadePaint.matrix;
        final Shader fade = mFadePaint.fade;

        if (drawLeft) {
            matrix.setScale(1, effectiveFadeLeft);
            matrix.postRotate(-90);
            matrix.postTranslate(left, mFadeTop);
            fade.setLocalMatrix(matrix);
            mFadePaint.setShader(fade);
            canvas.drawRect(left, mFadeTop, left + effectiveFadeLeft, mFadeBottom, mFadePaint);
        }

        if (drawRight) {
            matrix.setScale(1, effectiveFadeRight);
            matrix.postRotate(90);
            matrix.postTranslate(right, mFadeTop);
            fade.setLocalMatrix(matrix);
            mFadePaint.setShader(fade);
            canvas.drawRect(right - effectiveFadeRight, mFadeTop, right, mFadeBottom, mFadePaint);
        }
    }

    private void updateVerticalFadeBorders() {
        final View child = getChildAt(0);

        if (child != null) {
            mFadeTop = child.getTop() + child.getPaddingTop();
            mFadeBottom = child.getBottom() - child.getPaddingBottom();
        } else {
            mFadeTop = 0;
            mFadeBottom = 0;
        }

        mVerticalFadeBordersDirty = false;
    }

    private static final class FadePaint extends Paint {
        public final Matrix matrix;
        public final Shader fade;

        public FadePaint() {
            matrix = new Matrix();
            fade = new LinearGradient(0, 0, 0, 1, 0xFF000000, 0, Shader.TileMode.CLAMP);
            setShader(fade);
            setXfermode(new PorterDuffXfermode(PorterDuff.Mode.DST_OUT));
        }
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        // Once we intercept an event, this method stops being called until the intercept state
        // is reset at the start of the following gesture. Therefore we can reset the tracking
        // variable to false each time this method is being called.
        mInterceptingTouchEvents = false;

        final boolean intercept = super.onInterceptTouchEvent(ev);
        if (intercept) {
            mInterceptingTouchEvents = true;
        }
        return intercept;
    }

    @Override
    public boolean onTouchEvent(MotionEvent ev) {
        if (!mInterceptingTouchEvents) {
            final TouchDelegate touchDelegate = getTouchDelegate();
            if (touchDelegate != null && touchDelegate.onTouchEvent(ev)) {
                return true;
            }
        }

        return super.onTouchEvent(ev);
    }
}
