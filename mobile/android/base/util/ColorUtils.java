/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.content.Context;
import android.content.res.Resources;
import android.support.annotation.ColorRes;
import android.support.annotation.NonNull;

public class ColorUtils {
    // TODO: Eventually this functionality should be available in ContextCompat (support library)
    @SuppressWarnings("deprecation")
    public static int getColor(@NonNull Context context, @ColorRes int color) {
        final Resources resources = context.getResources();

        // TODO: After switching to Android M SDK: Use getColor(color, theme) on Android M+
        // if (AppConstants.Versions.feature23Plus) {
        //     return resources.getColor(color, context.getTheme());
        // }

        return resources.getColor(color);
    }
}
