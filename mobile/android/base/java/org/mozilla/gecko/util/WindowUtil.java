/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.app.Activity;
import android.graphics.Color;
import android.os.Build;
import android.support.annotation.ColorInt;
import android.support.annotation.ColorRes;
import android.support.v4.content.ContextCompat;
import android.view.View;
import android.view.Window;

import org.mozilla.gecko.GeckoApplication;
import org.mozilla.gecko.R;
import org.mozilla.gecko.lwt.LightweightTheme;

public class WindowUtil {

    public static void setTabsTrayStatusBarColor(final Activity activity) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            return;
        }
        setStatusBarColorRes(activity, R.color.status_bar_bg_color_tabs_tray, true);
    }

    public static void setStatusBarColor(final Activity activity, final boolean isPrivate) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            return;
        }

        final @ColorInt int color;
        final boolean isDarkTheme;

        if (HardwareUtils.isTablet()) {
            color = ContextCompat.getColor(activity, R.color.status_bar_bg_color_tablet);
            isDarkTheme = true;
        } else {
            LightweightTheme theme = ((GeckoApplication) activity.getApplication()).getLightweightTheme();
            @ColorInt int themeColor = theme.getColor();
            if (isPrivate) {
                color = ContextCompat.getColor(activity, R.color.status_bar_bg_color_private);
                isDarkTheme = true;
            } else if (theme.isEnabled() && themeColor != Color.TRANSPARENT) {
                color = themeColor;
                isDarkTheme = !theme.isLightTheme();
            } else {
                color = ContextCompat.getColor(activity, R.color.status_bar_bg_color);
                isDarkTheme = false;
            }
        }
        setStatusBarColor(activity, color, isDarkTheme);
    }

    public static void setStatusBarColorRes(final Activity activity, final @ColorRes int colorResId,
                                            final boolean isDarkTheme) {
        final int backgroundColor = ContextCompat.getColor(activity, colorResId);
        setStatusBarColor(activity, backgroundColor, isDarkTheme);
    }

    public static void setStatusBarColor(final Activity activity, final @ColorInt int backgroundColor,
                                         final boolean isDarkTheme) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            return;
        }

        final Window window = activity.getWindow();
        window.setStatusBarColor(backgroundColor);

        final View view = window.getDecorView();
        int flags = view.getSystemUiVisibility();
        if (isDarkTheme) {
            flags &= ~View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;
        } else {
            flags |= View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;
        }
        view.setSystemUiVisibility(flags);
    }
}
