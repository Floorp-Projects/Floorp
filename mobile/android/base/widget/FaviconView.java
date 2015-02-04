/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import org.mozilla.gecko.R;
import org.mozilla.gecko.favicons.Favicons;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.util.AttributeSet;
import android.widget.ImageView;
/**
 * Special version of ImageView for favicons.
 * Displays solid colour background around Favicon to fill space not occupied by the icon. Colour
 * selected is the dominant colour of the provided Favicon.
 */
public class FaviconView extends ImageView {
    private Bitmap mIconBitmap;

    // Reference to the unscaled bitmap, if any, to prevent repeated assignments of the same bitmap
    // to the view from causing repeated rescalings (Some of the callers do this)
    private Bitmap mUnscaledBitmap;

    // Key into the Favicon dominant colour cache. Should be the Favicon URL if the image displayed
    // here is a Favicon managed by the caching system. If not, any appropriately unique-to-this-image
    // string is acceptable.
    private String mIconKey;

    private int mActualWidth;
    private int mActualHeight;

    // Flag indicating if the most recently assigned image is considered likely to need scaling.
    private boolean mScalingExpected;

    // Dominant color of the favicon.
    private int mDominantColor;

    // Stroke width for the border.
    private static float sStrokeWidth;

    // Paint for drawing the stroke.
    private static final Paint sStrokePaint;

    // Paint for drawing the background.
    private static final Paint sBackgroundPaint;

    // Size of the stroke rectangle.
    private final RectF mStrokeRect;

    // Size of the background rectangle.
    private final RectF mBackgroundRect;

    // Type of the border whose value is defined in attrs.xml .
    private final boolean isDominantBorderEnabled;

    // boolean switch for overriding scaletype, whose value is defined in attrs.xml .
    private final boolean isOverrideScaleTypeEnabled;

    // Initializing the static paints.
    static {
        sStrokePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        sStrokePaint.setStyle(Paint.Style.STROKE);

        sBackgroundPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        sBackgroundPaint.setStyle(Paint.Style.FILL);
    }

    public FaviconView(Context context, AttributeSet attrs) {
        super(context, attrs);
        TypedArray a = context.getTheme().obtainStyledAttributes(attrs, R.styleable.FaviconView, 0, 0);

        try {
            isDominantBorderEnabled = a.getBoolean(R.styleable.FaviconView_dominantBorderEnabled, true);
            isOverrideScaleTypeEnabled = a.getBoolean(R.styleable.FaviconView_overrideScaleType, true);
        } finally {
            a.recycle();
        }

        if (isOverrideScaleTypeEnabled) {
            setScaleType(ImageView.ScaleType.CENTER);
        }

        mStrokeRect = new RectF();
        mBackgroundRect = new RectF();

        if (sStrokeWidth == 0) {
            sStrokeWidth = getResources().getDisplayMetrics().density;
            sStrokePaint.setStrokeWidth(sStrokeWidth);
        }

        mStrokeRect.left = mStrokeRect.top = sStrokeWidth;
        mBackgroundRect.left = mBackgroundRect.top = sStrokeWidth * 2.0f;
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh){
        super.onSizeChanged(w, h, oldw, oldh);

        // No point rechecking the image if there hasn't really been any change.
        if (w == mActualWidth && h == mActualHeight) {
            return;
        }

        mActualWidth = w;
        mActualHeight = h;

        mStrokeRect.right = w - sStrokeWidth;
        mStrokeRect.bottom = h - sStrokeWidth;
        mBackgroundRect.right = mStrokeRect.right - sStrokeWidth;
        mBackgroundRect.bottom = mStrokeRect.bottom - sStrokeWidth;

        formatImage();
    }

    @Override
    public void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        if (isDominantBorderEnabled) {
            // 27.5% transparent dominant color.
            sBackgroundPaint.setColor(mDominantColor & 0x46FFFFFF);
            canvas.drawRect(mStrokeRect, sBackgroundPaint);

            sStrokePaint.setColor(mDominantColor);
            canvas.drawRoundRect(mStrokeRect, sStrokeWidth, sStrokeWidth, sStrokePaint);
        }
    }

    /**
     * Formats the image for display, if the prerequisite data are available. Upscales tiny Favicons to
     * normal sized ones, replaces null bitmaps with the default Favicon, and fills all remaining space
     * in this view with the coloured background.
     */
    private void formatImage() {
        // If we're called before bitmap is set, or before size is set, show blank.
        if (mIconBitmap == null || mActualWidth == 0 || mActualHeight == 0) {
            showNoImage();
            return;
        }

        if (mScalingExpected && mActualWidth != mIconBitmap.getWidth()) {
            scaleBitmap();
            // Don't scale the image every time something changes.
            mScalingExpected = false;
        }

        setImageBitmap(mIconBitmap);

        // After scaling, determine if we have empty space around the scaled image which we need to
        // fill with the coloured background. If applicable, show it.
        // We assume Favicons are still squares and only bother with the background if more than 3px
        // of it would be displayed.
        if (Math.abs(mIconBitmap.getWidth() - mActualWidth) > 3) {
            mDominantColor = Favicons.getFaviconColor(mIconKey);
            if (mDominantColor == -1) {
                mDominantColor = 0;
            }
        } else {
            mDominantColor = 0;
        }
    }

    private void scaleBitmap() {
        // If the Favicon can be resized to fill the view exactly without an enlargment of more than
        // a factor of two, do so.
        int doubledSize = mIconBitmap.getWidth()*2;
        if (mActualWidth > doubledSize) {
            // If the view is more than twice the size of the image, just double the image size
            // and do the rest with padding.
            mIconBitmap = Bitmap.createScaledBitmap(mIconBitmap, doubledSize, doubledSize, true);
        } else {
            // Otherwise, scale the image to fill the view.
            mIconBitmap = Bitmap.createScaledBitmap(mIconBitmap, mActualWidth, mActualWidth, true);
        }
    }

    /**
     * Sets the icon displayed in this Favicon view to the bitmap provided. If the size of the view
     * has been set, the display will be updated right away, otherwise the update will be deferred
     * until then. The key provided is used to cache the result of the calculation of the dominant
     * colour of the provided image - this value is used to draw the coloured background in this view
     * if the icon is not large enough to fill it.
     *
     * @param bitmap favicon image
     * @param key string used as a key to cache the dominant color of this image
     * @param allowScaling If true, allows the provided bitmap to be scaled by this FaviconView.
     *                     Typically, you should prefer using Favicons obtained via the caching system
     *                     (Favicons class), so as to exploit caching.
     */
    private void updateImageInternal(Bitmap bitmap, String key, boolean allowScaling) {
        if (bitmap == null) {
            showDefaultFavicon();
            return;
        }

        // Reassigning the same bitmap? Don't bother.
        if (mUnscaledBitmap == bitmap) {
            return;
        }
        mUnscaledBitmap = bitmap;
        mIconBitmap = bitmap;
        mIconKey = key;
        mScalingExpected = allowScaling;

        // Possibly update the display.
        formatImage();
    }

    public void showDefaultFavicon() {
        setImageResource(R.drawable.favicon_globe);
        mDominantColor = 0;
    }

    private void showNoImage() {
        setImageDrawable(null);
        mDominantColor = 0;
    }

    /**
     * Clear image and background shown by this view.
     */
    public void clearImage() {
        showNoImage();
        mUnscaledBitmap = null;
        mIconBitmap = null;
        mIconKey = null;
        mScalingExpected = false;
    }

    /**
     * Update the displayed image and apply the scaling logic.
     * The scaling logic will attempt to resize the image to fit correctly inside the view in a way
     * that avoids unreasonable levels of loss of quality.
     * Scaling is necessary only when the icon being provided is not drawn from the Favicon cache
     * introduced in Bug 914296.
     *
     * Due to Bug 913746, icons bundled for search engines are not available to the cache, so must
     * always have the scaling logic applied here. At the time of writing, this is the only case in
     * which the scaling logic here is applied.
     *
     * @param bitmap The bitmap to display in this favicon view.
     * @param key The key to use into the dominant colours cache when selecting a background colour.
     */
    public void updateAndScaleImage(Bitmap bitmap, String key) {
        updateImageInternal(bitmap, key, true);
    }

    /**
     * Update the image displayed in the Favicon view without scaling. Images larger than the view
     * will be centrally cropped. Images smaller than the view will be placed centrally and the
     * extra space filled with the dominant colour of the provided image.
     *
     * @param bitmap The bitmap to display in this favicon view.
     * @param key The key to use into the dominant colours cache when selecting a background colour.
     */
    public void updateImage(Bitmap bitmap, String key) {
        updateImageInternal(bitmap, key, false);
    }

    public Bitmap getBitmap() {
        return mIconBitmap;
    }
}
