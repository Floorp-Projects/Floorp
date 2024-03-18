/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.view

import android.os.Build
import android.os.Build.VERSION.SDK_INT
import android.view.Window
import androidx.annotation.ColorInt
import androidx.core.view.WindowInsetsControllerCompat
import mozilla.components.support.utils.ColorUtils.isDark

/**
 * Sets the status bar background color. If the color is light enough, a light navigation bar with
 * dark icons will be used.
 */
fun Window.setStatusBarTheme(@ColorInt color: Int) {
    createWindowInsetsController().isAppearanceLightStatusBars = !isDark(color)
    statusBarColor = color
}

/**
 * Set the navigation bar background and divider colors. If the color is light enough, a light
 * navigation bar with dark icons will be used.
 */
fun Window.setNavigationBarTheme(
    @ColorInt navBarColor: Int? = null,
    @ColorInt navBarDividerColor: Int? = null,
) {
    navBarColor?.let {
        navigationBarColor = it
        createWindowInsetsController().isAppearanceLightNavigationBars = !isDark(it)
    }

    if (SDK_INT >= Build.VERSION_CODES.P) {
        navigationBarDividerColor = navBarDividerColor ?: 0
    }
}

/**
 * Creates a {@link WindowInsetsControllerCompat} for the top-level window decor view.
 */
fun Window.createWindowInsetsController(): WindowInsetsControllerCompat {
    return WindowInsetsControllerCompat(this, this.decorView)
}
