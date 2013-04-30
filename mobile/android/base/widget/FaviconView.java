/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import org.mozilla.gecko.Favicons;
import org.mozilla.gecko.R;

import android.content.Context;
import android.graphics.Bitmap;
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

    @Override
    public void setImageBitmap(Bitmap bitmap) {
        if (bitmap == null) {
            // Call setImageDrawable directly to avoid creating a useless BitmapDrawable.
            setImageDrawable(null);
            // If the bitmap is null, show a blank background.
            setBackgroundResource(R.drawable.favicon_bg);
        } else if (Favicons.getInstance().isLargeFavicon(bitmap)) {
            super.setImageBitmap(bitmap);
            // If the icon is large, hide the background.
            setBackgroundResource(0);
        } else {
            super.setImageBitmap(bitmap);
            // XXX Otherwise show a dominant color background.
            setBackgroundResource(R.drawable.favicon_bg);
        }
    }
}
