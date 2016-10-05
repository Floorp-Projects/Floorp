/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.activities;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.preference.Preference;
import android.support.v4.graphics.drawable.RoundedBitmapDrawable;
import android.support.v4.graphics.drawable.RoundedBitmapDrawableFactory;
import com.squareup.picasso.Picasso;
import com.squareup.picasso.Target;
import org.mozilla.gecko.AppConstants;

/**
 * A Picasso Target that updates a preference icon.
 *
 * Nota bene: Android grew support for updating preference icons programatically
 * only in API 11.  This class silently ignores requests before API 11.
 */
public class PicassoPreferenceIconTarget implements Target {
    private final Preference preference;
    private final Resources resources;
    private final float cornerRadius;

    public PicassoPreferenceIconTarget(Resources resources, Preference preference) {
        this(resources, preference, 0);
    }

    public PicassoPreferenceIconTarget(Resources resources, Preference preference, float cornerRadius) {
        this.resources = resources;
        this.preference = preference;
        this.cornerRadius = cornerRadius;
    }

    @Override
    public void onBitmapLoaded(Bitmap bitmap, Picasso.LoadedFrom from) {
        final Drawable drawable;
        if (cornerRadius > 0) {
            final RoundedBitmapDrawable roundedBitmapDrawable;
            roundedBitmapDrawable = RoundedBitmapDrawableFactory.create(resources, bitmap);
            roundedBitmapDrawable.setCornerRadius(cornerRadius);
            roundedBitmapDrawable.setAntiAlias(true);
            drawable = roundedBitmapDrawable;
        } else {
            drawable = new BitmapDrawable(resources, bitmap);
        }
        preference.setIcon(drawable);
    }

    @Override
    public void onBitmapFailed(Drawable errorDrawable) {
        preference.setIcon(errorDrawable);
    }

    @Override
    public void onPrepareLoad(Drawable placeHolderDrawable) {
        preference.setIcon(placeHolderDrawable);
    }
}
