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
 * Colors the status bar.
 * If the color is light enough, a light status bar with dark icons will be used.
 */
fun Window.setStatusBarTheme(@ColorInt toolbarColor: Int) {
    getWindowInsetsController().isAppearanceLightStatusBars =
        !isDark(toolbarColor)
    statusBarColor = toolbarColor
}

/**
 * Colors the navigation bar.
 * If the color is light enough, a light navigation bar with dark icons will be used.
 */
fun Window.setNavigationBarTheme(@ColorInt toolbarColor: Int) {
    getWindowInsetsController().isAppearanceLightNavigationBars =
        !isDark(toolbarColor)

    if (SDK_INT >= Build.VERSION_CODES.P) {
        navigationBarDividerColor = 0
    }
    navigationBarColor = toolbarColor
}

/**
 * Retrieves a {@link WindowInsetsControllerCompat} for the top-level window decor view.
 */
fun Window.getWindowInsetsController(): WindowInsetsControllerCompat {
    return WindowInsetsControllerCompat(this, this.decorView)
}
