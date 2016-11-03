/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.support.annotation.CheckResult;
import android.support.annotation.ColorInt;
import android.support.annotation.ColorRes;
import android.support.annotation.DrawableRes;
import android.support.annotation.NonNull;
import android.support.v4.content.ContextCompat;
import android.support.v4.graphics.drawable.DrawableCompat;
import android.support.v7.widget.AppCompatDrawableManager;

import org.mozilla.gecko.AppConstants;

public class DrawableUtil {

    /**
     * Tints the given drawable with the given color and returns it.
     */
    @CheckResult
    public static Drawable tintDrawable(@NonNull final Context context,
                                        @DrawableRes final int drawableID,
                                        @ColorInt final int color) {
        final Drawable icon = DrawableCompat.wrap(
                AppCompatDrawableManager.get().getDrawable(context, drawableID).mutate());
        DrawableCompat.setTint(icon, color);
        return icon;
    }

    /**
     * Tints the given drawable with the given color and returns it.
     */
    @CheckResult
    public static Drawable tintDrawableWithColorRes(@NonNull final Context context,
                                                    @DrawableRes final int drawableID,
                                                    @ColorRes final int colorID) {
        return tintDrawable(context, drawableID, ContextCompat.getColor(context, colorID));
    }

    /**
     * Tints the given drawable with the given tint list and returns it. Note that you
     * should no longer use the argument Drawable because the argument is not mutated
     * on pre-Lollipop devices but is mutated on L+ due to differences in the Support
     * Library implementation (bug 1193950).
     */
    @CheckResult
    public static Drawable tintDrawableWithStateList(@NonNull final Drawable drawable,
            @NonNull final ColorStateList colorList) {
        final Drawable wrappedDrawable = DrawableCompat.wrap(drawable.mutate());
        DrawableCompat.setTintList(wrappedDrawable, colorList);

        // DrawableCompat on pre-L doesn't handle its bounds correctly, and by default therefore won't
        // be rendered - we need to manually copy the bounds as a workaround:
        if (AppConstants.Versions.preMarshmallow) {
            wrappedDrawable.setBounds(0, 0, wrappedDrawable.getIntrinsicHeight(), wrappedDrawable.getIntrinsicHeight());
        }

        return wrappedDrawable;
    }
}
