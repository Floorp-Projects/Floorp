/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.view

import android.os.Build
import android.os.Build.VERSION.SDK_INT
import android.view.View
import android.view.Window
import androidx.annotation.ColorInt
import mozilla.components.support.utils.ColorUtils.isDark

/**
 * Colors the status bar.
 * If the color is light enough, a light status bar with dark icons will be used.
 */
fun Window.setStatusBarTheme(@ColorInt color: Int) {
    if (SDK_INT >= Build.VERSION_CODES.M) {
        val flags = decorView.systemUiVisibility
        decorView.systemUiVisibility =
            flags.useLightFlag(color, View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR)
    }
    statusBarColor = color
}

/**
 * Colors the navigation bar.
 * If the color is light enough, a light navigation bar with dark icons will be used.
 */
fun Window.setNavigationBarTheme(@ColorInt color: Int) {
    if (SDK_INT >= Build.VERSION_CODES.O) {
        val flags = decorView.systemUiVisibility
        decorView.systemUiVisibility =
            flags.useLightFlag(color, View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR)
    }
    if (SDK_INT >= Build.VERSION_CODES.P) {
        navigationBarDividerColor = 0
    }
    navigationBarColor = color
}

/**
 * Augment this flag int with the addition or removal of a light status/navigation bar flag.
 */
private fun Int.useLightFlag(@ColorInt baseColor: Int, lightFlag: Int): Int {
    return if (isDark(baseColor)) {
        // Use dark bar
        this and lightFlag.inv()
    } else {
        // Use light bar
        this or lightFlag
    }
}
