/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.support.ktx.android.view

import android.graphics.Color
import android.os.Build
import android.view.View
import android.view.Window
import androidx.annotation.ColorInt
import mozilla.components.support.utils.ColorUtils.getReadableTextColor

/**
 * Colors the status bar to match the theme color.
 * If the color is light enough, a light status bar with dark icons will be used.
 */
fun Window.setStatusBarTheme(@ColorInt themeColor: Int) {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
        val flags = decorView.systemUiVisibility
        decorView.systemUiVisibility =
            flags.useLightFlag(themeColor, View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR)
    }
    statusBarColor = themeColor
}

/**
 * Colors the navigation bar black or white depending on the background color.
 */
fun Window.setNavigationBarTheme(@ColorInt backgroundColor: Int) {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
        val flags = decorView.systemUiVisibility
        decorView.systemUiVisibility =
            flags.useLightFlag(backgroundColor, View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR)
    }
}

/**
 * Augment this flag int with the addition or removal of a light status/navigation bar flag.
 */
private fun Int.useLightFlag(@ColorInt baseColor: Int, lightFlag: Int): Int {
    return if (getReadableTextColor(baseColor) == Color.BLACK) {
        // Use light bar
        this or lightFlag
    } else {
        // Use dark bar
        this and lightFlag.inv()
    }
}
