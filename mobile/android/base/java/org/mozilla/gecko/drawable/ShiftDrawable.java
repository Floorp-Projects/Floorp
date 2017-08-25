package org.mozilla.gecko.drawable;

/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import android.animation.Animator;
import android.animation.ValueAnimator;
import android.graphics.Canvas;
import android.graphics.Path;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.view.animation.Interpolator;
import android.view.animation.LinearInterpolator;

/**
 * A drawable to keep shifting its wrapped drawable.
 * Assume the wrapped drawable value is "00000010", this class will keep drawing in this way
 * <p>
 * 00000010 -> 00000001 -> 10000000 -> 01000000 -> 00100000 -> ...
 * <p>
 * This drawable will keep drawing until be invisible.
 */
public class ShiftDrawable extends DrawableWrapper {

    /**
     * An animator to trigger redraw and update offset-of-shifting
     */
    private final ValueAnimator mAnimator = ValueAnimator.ofFloat(0f, 1f);

    /**
     * Visible rectangle, wrapped-drawable is resized and draw in this rectangle
     */
    private final Rect mVisibleRect = new Rect();

    /**
     * Canvas will clip itself by this Path. Used to draw rounded head.
     */
    private final Path mPath = new Path();

    // align to ScaleDrawable implementation
    private static final int MAX_LEVEL = 10000;

    private static final int DEFAULT_DURATION = 1000;

    public ShiftDrawable(@NonNull Drawable d) {
        this(d, DEFAULT_DURATION);
    }

    public ShiftDrawable(@NonNull Drawable d, int duration) {
        this(d, duration, new LinearInterpolator());
    }

    public ShiftDrawable(@NonNull Drawable d, int duration, @Nullable Interpolator interpolator) {
        super(d);
        mAnimator.setDuration(duration);
        mAnimator.setRepeatCount(ValueAnimator.INFINITE);
        mAnimator.setInterpolator((interpolator == null) ? new LinearInterpolator() : interpolator);
        mAnimator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
            @Override
            public void onAnimationUpdate(ValueAnimator animation) {
                if (isVisible()) {
                    invalidateSelf();
                }
            }
        });
        mAnimator.start();
    }

    public Animator getAnimator() {
        return mAnimator;
    }

    /**
     * {@inheritDoc}
     * <p>
     * override to enable / disable animator as well.
     */
    @Override
    public boolean setVisible(final boolean visible, final boolean restart) {
        final boolean result = super.setVisible(visible, restart);
        if (isVisible()) {
            mAnimator.start();
        } else {
            mAnimator.end();
        }
        return result;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    protected void onBoundsChange(Rect bounds) {
        super.onBoundsChange(bounds);
        updateBounds();
    }

    /**
     * {@inheritDoc}
     */
    @Override
    protected boolean onLevelChange(int level) {
        final boolean result = super.onLevelChange(level);
        updateBounds();
        return result;
    }

    @Override
    public void draw(@NonNull Canvas canvas) {
        final Drawable wrapped = getWrappedDrawable();
        final float fraction = mAnimator.getAnimatedFraction();
        final int width = mVisibleRect.width();
        final int offset = (int) (width * fraction);

        final int stack = canvas.save();

        // To apply mPath, then we have rounded-head
        canvas.clipPath(mPath);

        // To draw left-half part of Drawable, shift from right to left
        canvas.save();
        canvas.translate(-offset, 0);
        wrapped.draw(canvas);
        canvas.restore();

        // Then to draw right-half part of Drawable
        canvas.save();
        canvas.translate(width - offset, 0);
        wrapped.draw(canvas);
        canvas.restore();

        canvas.restoreToCount(stack);
    }

    private void updateBounds() {
        final Rect b = getBounds();
        final int width = (int) ((float) b.width() * getLevel() / MAX_LEVEL);
        mVisibleRect.set(b.left, b.top, b.left + width, b.height());

        // to create path to help drawing rounded head. mPath is enclosed by mVisibleRect
        final float radius = b.height() / 2;
        mPath.reset();

        // The added rectangle width is smaller than mVisibleRect, due to semi-circular.
        mPath.addRect(mVisibleRect.left,
                mVisibleRect.top, mVisibleRect.right - radius,
                mVisibleRect.height(),
                Path.Direction.CCW);
        // To add semi-circular
        mPath.addCircle(mVisibleRect.right - radius, radius, radius, Path.Direction.CCW);
    }
}
