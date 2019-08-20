/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.app.Activity
import android.content.Context
import android.content.res.Configuration
import android.graphics.Color
import android.os.Build
import android.os.Build.VERSION.SDK_INT
import android.util.TypedValue
import android.view.View
import android.view.Window
import androidx.annotation.StyleRes
import org.mozilla.fenix.ext.getColorFromAttr

abstract class ThemeManager {

    abstract var currentTheme: BrowsingMode

    /**
     * Returns the style resource corresponding to the [currentTheme].
     */
    @get:StyleRes
    val currentThemeResource get() = when (currentTheme) {
        BrowsingMode.Normal -> R.style.NormalTheme
        BrowsingMode.Private -> R.style.PrivateTheme
    }

    /**
     * Handles status bar theme change since the window does not dynamically recreate
     */
    fun applyStatusBarTheme(activity: Activity) = applyStatusBarTheme(activity.window, activity)
    fun applyStatusBarTheme(window: Window, context: Context) {
        when (currentTheme) {
            BrowsingMode.Normal -> {
                when (context.resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK) {
                    Configuration.UI_MODE_NIGHT_UNDEFINED, // We assume light here per Android doc's recommendation
                    Configuration.UI_MODE_NIGHT_NO -> {
                        updateLightSystemBars(window, context)
                    }
                    Configuration.UI_MODE_NIGHT_YES -> {
                        clearLightSystemBars(window)
                        updateNavigationBar(window, context)
                    }
                }
            }
            BrowsingMode.Private -> {
                clearLightSystemBars(window)
                updateNavigationBar(window, context)
            }
        }
    }

    fun setActivityTheme(activity: Activity) {
        activity.setTheme(currentThemeResource)
    }

    companion object {
        fun resolveAttribute(attribute: Int, context: Context): Int {
            val typedValue = TypedValue()
            val theme = context.theme
            theme.resolveAttribute(attribute, typedValue, true)

            return typedValue.resourceId
        }

        private fun updateLightSystemBars(window: Window, context: Context) {
            if (SDK_INT >= Build.VERSION_CODES.M) {
                window.statusBarColor = context.getColorFromAttr(android.R.attr.statusBarColor)

                window.decorView.systemUiVisibility =
                    window.decorView.systemUiVisibility or View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR
            } else {
                window.statusBarColor = Color.BLACK
            }

            if (SDK_INT >= Build.VERSION_CODES.O) {
                // API level can display handle light navigation bar color
                window.decorView.systemUiVisibility =
                    window.decorView.systemUiVisibility or View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR
                updateNavigationBar(window, context)
            }
        }

        private fun clearLightSystemBars(window: Window) {
            if (SDK_INT >= Build.VERSION_CODES.M) {
                window.decorView.systemUiVisibility = window.decorView.systemUiVisibility and
                        View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR.inv() and
                        View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR.inv()
            }
        }

        private fun updateNavigationBar(window: Window, context: Context) {
            window.navigationBarColor = context.getColorFromAttr(R.attr.foundation)
        }
    }
}

class DefaultThemeManager(
    currentTheme: BrowsingMode,
    private val activity: Activity
) : ThemeManager() {
    override var currentTheme: BrowsingMode = currentTheme
        set(value) {
            if (currentTheme != value) {
                field = value

                setActivityTheme(activity)
                activity.recreate()
            }
        }
}

class CustomTabThemeManager : ThemeManager() {
    override var currentTheme
        get() = BrowsingMode.Normal
        set(_) { /* noop */ }
}
