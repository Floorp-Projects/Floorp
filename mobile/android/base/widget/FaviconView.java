/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import org.mozilla.gecko.Favicons;
import org.mozilla.gecko.R;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.PorterDuff.Mode;
import android.graphics.drawable.Drawable;
import android.os.AsyncTask;
import android.util.AttributeSet;
import android.widget.ImageView;

/*
 * Special version of ImageView for favicons.
 * Changes image background depending on favicon size.
 */
public class FaviconView extends ImageView {

    public FaviconView(Context context, AttributeSet attrs) {
        super(context, attrs);
        setScaleType(ImageView.ScaleType.CENTER);
    }

    /*
     * @param bitmap favicon image
     * @param key string used as a key to cache the dominant color of this image
     */
    public void updateImage(Bitmap bitmap, String key) {
        if (bitmap == null) {
            // If the bitmap is null, show the default favicon.
            setImageResource(R.drawable.favicon);
            // The default favicon image is hi-res, so we should hide the background.
            setBackgroundResource(0);
        } else if (Favicons.getInstance().isLargeFavicon(bitmap)) {
            setImageBitmap(bitmap);
            // If the icon is large, hide the background.
            setBackgroundResource(0);
        } else {
            setImageBitmap(bitmap);
            // Otherwise show a dominant color background.
            int color = Favicons.getInstance().getFaviconColor(bitmap, key);
            color = Color.argb(70, Color.red(color), Color.green(color), Color.blue(color));
            Drawable drawable = getResources().getDrawable(R.drawable.favicon_bg);
            drawable.setColorFilter(color, Mode.SRC_ATOP);
            setBackgroundDrawable(drawable);
        }
    }
}
