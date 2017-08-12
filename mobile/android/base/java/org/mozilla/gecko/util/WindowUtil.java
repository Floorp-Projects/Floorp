/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.app.Activity;
import android.os.Build;
import android.support.v4.content.ContextCompat;
import android.view.View;
import android.view.Window;

import org.mozilla.gecko.R;

public class WindowUtil {

    public static void invalidateStatusBarColor(final Activity activity, boolean darkTheme) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            return;
        }

        final int colorResId;

        if (HardwareUtils.isTablet()) {
            colorResId = R.color.status_bar_bg_color_tablet;
            darkTheme = true;
        } else {
            colorResId = darkTheme ? R.color.status_bar_bg_color_private : R.color.status_bar_bg_color;
        }

        final Window window = activity.getWindow();
        final int backgroundColor = ContextCompat.getColor(activity, colorResId);
        window.setStatusBarColor(backgroundColor);

        final View view = window.getDecorView();
        int flags = view.getSystemUiVisibility();
        if (darkTheme) {
            flags &= ~View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;
        } else {
            flags |= View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;
        }
        view.setSystemUiVisibility(flags);
    }
}
