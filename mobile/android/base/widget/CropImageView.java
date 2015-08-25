/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Matrix;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.widget.ImageView;

import com.nineoldandroids.view.ViewHelper;

/**
 * An ImageView which will always display at the given width and calculated height (based on the width and
 * the supplied aspect ratio), drawn starting from the top left hand corner.  A supplied drawable will be resized to fit
 * the width of the view; if the resized drawable is too tall for the view then the drawable will be cropped at the
 * bottom, however if the resized drawable is too short for the view to display whilst honouring it's given width and
 * height then the drawable will be displayed at full height with the right hand side cropped.
 */
public abstract class CropImageView extends ThemedImageView {
    public static final String LOGTAG = "Gecko" + CropImageView.class.getSimpleName();

    private int viewWidth;
    private int viewHeight;
    private int drawableWidth;
    private int drawableHeight;

    private boolean resize = true;
    private Matrix layoutCurrentMatrix = new Matrix();
    private Matrix layoutNextMatrix = new Matrix();


    public CropImageView(final Context context) {
        this(context, null);
    }

    public CropImageView(final Context context, final AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public CropImageView(final Context context, final AttributeSet attrs, final int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }

    protected abstract float getAspectRatio();

    protected void init() {
        // Setting the pivots means that the image will be drawn from the top left hand corner.  There are
        // issues in Android 4.1 (16) which mean setting these values to 0 may not work.
        // http://stackoverflow.com/questions/26658124/setpivotx-doesnt-work-on-android-4-1-1-nineoldandroids
        ViewHelper.setPivotX(this, 1);
        ViewHelper.setPivotY(this, 1);
    }

    /**
     * Measure the view to determine the measured width and height.
     * The height is constrained by the measured width.
     *
     * @param widthMeasureSpec  horizontal space requirements as imposed by the parent.
     * @param heightMeasureSpec vertical space requirements as imposed by the parent, but ignored.
     */
    @Override
    protected void onMeasure(final int widthMeasureSpec, final int heightMeasureSpec) {
        // Default measuring.
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        // Force the height based on the aspect ratio.
        viewWidth = getMeasuredWidth();
        viewHeight = (int) (viewWidth * getAspectRatio());

        setMeasuredDimension(viewWidth, viewHeight);

        updateImageMatrix();
    }

    protected void updateImageMatrix() {
        if (!resize || getDrawable() == null) {
            return;
        }

        setScaleType(ImageView.ScaleType.MATRIX);

        getDrawable().setBounds(0, 0, viewWidth, viewHeight);

        final float horizontalScaleValue = (float) viewWidth / (float) drawableWidth;
        final float verticalScaleValue = (float) viewHeight / (float) drawableHeight;

        final float scale = Math.max(verticalScaleValue, horizontalScaleValue);

        layoutNextMatrix.setScale(scale, scale);
        setImageMatrix(layoutNextMatrix);

        // You can't modify the matrix in place and we want to avoid allocation, so let's keep two references to two
        // different matrix objects that we can swap when the values need to change
        final Matrix swapReferenceMatrix = layoutCurrentMatrix;
        layoutCurrentMatrix = layoutNextMatrix;
        layoutNextMatrix = swapReferenceMatrix;
    }

    public void setImageBitmap(final Bitmap bm, final boolean resize) {
        super.setImageBitmap(bm);

        this.resize = resize;
        updateImageMatrix();
    }

    @Override
    public void setImageResource(final int resId) {
        super.setImageResource(resId);
        setImageMatrix(null);
        resize = false;
    }

    @Override
    public void setImageDrawable(final Drawable drawable) {
        this.setImageDrawable(drawable, false);
    }

    public void setImageDrawable(final Drawable drawable, final boolean resize) {
        super.setImageDrawable(drawable);

        if (drawable != null) {
            // Reset the matrix to ensure that any previous changes aren't carried through.
            setImageMatrix(null);

            drawableWidth = drawable.getIntrinsicWidth();
            drawableHeight = drawable.getIntrinsicHeight();
        } else {
            drawableWidth = -1;
            drawableHeight = -1;
        }

        this.resize = resize;

        updateImageMatrix();
    }
}
