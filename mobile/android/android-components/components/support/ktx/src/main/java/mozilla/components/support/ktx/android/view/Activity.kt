/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.support.ktx.android.view

import android.app.Activity
import android.graphics.Color
import android.os.Build
import android.view.View
import android.view.WindowManager
import androidx.annotation.ColorInt
import mozilla.components.support.utils.ColorUtils.getReadableTextColor

/**
 * Attempts to call immersive mode using the View to hide the status bar and navigation buttons.
 */
fun Activity.enterToImmersiveMode() {
    window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
    window.decorView.systemUiVisibility = (View.SYSTEM_UI_FLAG_LAYOUT_STABLE
        or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
        or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
        or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
        or View.SYSTEM_UI_FLAG_FULLSCREEN
        or View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY)
}

/**
 * Attempts to come out from immersive mode using the View.
 */
fun Activity.exitImmersiveModeIfNeeded() {
    if (WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON and window.attributes.flags == 0) {
        // We left immersive mode already.
        return
    }

    window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
    window.decorView.systemUiVisibility = View.SYSTEM_UI_FLAG_VISIBLE
}

/**
 * Colors the status bar to match the theme color.
 * If the color is light enough, a light status bar with dark icons will be used.
 */
fun Activity.setStatusBarTheme(@ColorInt themeColor: Int) {
    if (Build.VERSION.SDK_INT > Build.VERSION_CODES.M) {
        val flags = window.decorView.systemUiVisibility
        window.decorView.systemUiVisibility =
            flags.useLightFlag(themeColor, View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR)
    }
    window.statusBarColor = themeColor
}

/**
 * Colors the navigation bar black or white depending on the background color.
 */
fun Activity.setNavigationBarTheme(@ColorInt backgroundColor: Int) {
    if (Build.VERSION.SDK_INT > Build.VERSION_CODES.O) {
        val flags = window.decorView.systemUiVisibility
        window.decorView.systemUiVisibility =
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
