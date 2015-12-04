/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import org.mozilla.gecko.R;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.support.v4.content.ContextCompat;
import org.mozilla.gecko.widget.themed.ThemedImageView;

/* Special version of ImageView for thumbnails. Scales a thumbnail so that it maintains its aspect
 * ratio and so that the images width and height are the same size or greater than the view size
 */
public class ThumbnailView extends ThemedImageView {
    private static final String LOGTAG = "GeckoThumbnailView";

    final private Matrix mMatrix;
    private int mWidthSpec = -1;
    private int mHeightSpec = -1;
    private boolean mLayoutChanged;
    private boolean mScale = false;

    public ThumbnailView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mMatrix = new Matrix();
        mLayoutChanged = true;
    }

    @Override
    public void onDraw(Canvas canvas) {
        if (!mScale) {
            super.onDraw(canvas);
            return;
        }

        Drawable d = getDrawable();
        if (mLayoutChanged) {
            int w1 = d.getIntrinsicWidth();
            int h1 = d.getIntrinsicHeight();
            int w2 = getWidth();
            int h2 = getHeight();

            float scale = (w2/h2 < w1/h1) ? (float)h2/h1 : (float)w2/w1;
            mMatrix.setScale(scale, scale);
        }

        int saveCount = canvas.save();
        canvas.concat(mMatrix);
        d.draw(canvas);
        canvas.restoreToCount(saveCount);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        // OnLayout.changed isn't a reliable measure of whether or not the size of this view has changed
        // neither is onSizeChanged called often enough. Instead, we track changes in size ourselves, and
        // only invalidate this matrix if we have a new width/height spec
        if (widthMeasureSpec != mWidthSpec || heightMeasureSpec != mHeightSpec) {
            mWidthSpec = widthMeasureSpec;
            mHeightSpec = heightMeasureSpec;
            mLayoutChanged = true;
        }
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    @Override
    public void setImageDrawable(Drawable drawable) {
        if (drawable == null) {
            drawable = ContextCompat.getDrawable(getContext(), R.drawable.tab_panel_tab_background);
            setScaleType(ScaleType.FIT_XY);
            mScale = false;
        } else {
            mScale = true;
            setScaleType(ScaleType.FIT_CENTER);
        }

        super.setImageDrawable(drawable);
    }
}
