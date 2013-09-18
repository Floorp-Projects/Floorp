/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import org.mozilla.gecko.favicons.Favicons;
import org.mozilla.gecko.R;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.PorterDuff.Mode;
import android.graphics.drawable.Drawable;
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

    public FaviconView(Context context, AttributeSet attrs) {
        super(context, attrs);
        setScaleType(ImageView.ScaleType.CENTER);
    }

    @Override
    protected void onSizeChanged(int xNew, int yNew, int xOld, int yOld){
        super.onSizeChanged(xNew, yNew, xOld, yOld);

        // No point rechecking the image if there hasn't really been any change.
        if (xNew == mActualHeight && yNew == mActualWidth) {
            return;
        }
        mActualWidth = xNew;
        mActualHeight = yNew;
        formatImage();
    }

    /**
     * Formats the image for display, if the prerequisite data are available. Upscales tiny Favicons to
     * normal sized ones, replaces null bitmaps with the default Favicon, and fills all remaining space
     * in this view with the coloured background.
     */
    private void formatImage() {
        // If we're called before bitmap is set, just show the default.
        if (mIconBitmap == null) {
            setImageResource(0);
            hideBackground();
            return;
        }

        // If we're called before size set, abort.
        if (mActualWidth == 0 || mActualHeight == 0) {
            hideBackground();
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
            showBackground();
        } else {
            hideBackground();
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
     * Helper method to display background of the dominant colour of the favicon to pad the remaining
     * space.
     */
    private void showBackground() {
        int color = Favicons.getFaviconColor(mIconBitmap, mIconKey);
        color = Color.argb(70, Color.red(color), Color.green(color), Color.blue(color));
        final Drawable drawable = getResources().getDrawable(R.drawable.favicon_bg);
        drawable.setColorFilter(color, Mode.SRC_ATOP);
        setBackgroundDrawable(drawable);
    }

    /**
     * Method to hide the background. The view will now have a transparent background.
     */
    private void hideBackground() {
        setBackgroundResource(0);
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

    /**
     * Clear image and background shown by this view.
     */
    public void clearImage() {
        setImageResource(0);
        hideBackground();
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
