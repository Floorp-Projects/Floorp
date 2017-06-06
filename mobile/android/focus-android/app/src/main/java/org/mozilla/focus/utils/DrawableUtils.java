/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.support.annotation.ColorInt;
import android.support.annotation.DrawableRes;
import android.support.annotation.NonNull;
import android.support.v4.graphics.drawable.DrawableCompat;

public class DrawableUtils {
    public static Drawable loadAndTintDrawable(@NonNull Context context, @DrawableRes int resourceId, @ColorInt  int color) {
        final Drawable drawable = context.getResources().getDrawable(resourceId, context.getTheme());
        final Drawable wrapped = DrawableCompat.wrap(drawable.mutate());
        DrawableCompat.setTint(wrapped, color);
        return wrapped;
    }
}
