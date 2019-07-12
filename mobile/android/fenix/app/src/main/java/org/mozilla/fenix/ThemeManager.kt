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
import androidx.core.content.ContextCompat

interface ThemeManager {
    enum class Theme {
        Normal, Private;

        fun isPrivate(): Boolean = this == Private
    }

    val currentTheme: Theme
    fun setTheme(theme: Theme)

    companion object {
        fun resolveAttribute(attribute: Int, context: Context): Int {
            val typedValue = TypedValue()
            val theme = context.theme
            theme.resolveAttribute(attribute, typedValue, true)

            return typedValue.resourceId
        }

        // Handles status bar theme change since the window does not dynamically recreate
        fun applyStatusBarTheme(
            window: Window,
            themeManager: ThemeManager,
            context: Context
        ) {
            when (themeManager.currentTheme) {
                ThemeManager.Theme.Normal -> {
                    when (context.resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK) {
                        Configuration.UI_MODE_NIGHT_NO -> {
                            updateLightSystemBars(window, context)
                        }
                        Configuration.UI_MODE_NIGHT_YES -> {
                            window.decorView.systemUiVisibility =
                                window.decorView.systemUiVisibility and
                                        View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR.inv() and
                                        View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR.inv()
                            updateNavigationBar(window, context)
                        }
                        Configuration.UI_MODE_NIGHT_UNDEFINED -> {
                            // We assume light here per Android doc's recommendation
                            updateLightSystemBars(window, context)
                        }
                    }
                }
                ThemeManager.Theme.Private -> {
                    window.decorView.systemUiVisibility = window.decorView.systemUiVisibility and
                            View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR.inv() and
                            View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR.inv()
                    updateNavigationBar(window, context)
                }
            }
        }

        private fun updateLightSystemBars(
            window: Window,
            context: Context
        ) {
            if (SDK_INT >= Build.VERSION_CODES.M) {
                window.statusBarColor = ContextCompat
                    .getColor(
                        context, resolveAttribute(android.R.attr.statusBarColor, context)
                    )

                window.decorView.systemUiVisibility =
                    window.decorView.systemUiVisibility or
                            View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR
            } else {
                window.statusBarColor = Color.BLACK
            }

            if (SDK_INT >= Build.VERSION_CODES.O) {
                // API level can display handle light navigation bar color
                window.decorView.systemUiVisibility =
                    window.decorView.systemUiVisibility or
                            View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR
                updateNavigationBar(window, context)
            }
        }

        private fun updateNavigationBar(
            window: Window,
            context: Context
        ) {
            window.navigationBarColor = ContextCompat
                .getColor(
                    context, resolveAttribute(R.attr.foundation, context)
                )
        }
    }
}

val ThemeManager.currentThemeResource: Int
    get() = when (currentTheme) {
        ThemeManager.Theme.Normal -> R.style.NormalTheme
        ThemeManager.Theme.Private -> R.style.PrivateTheme
    }

fun Activity.setTheme(theme: ThemeManager.Theme) {
    val themeCode = when (theme) {
        ThemeManager.Theme.Normal -> R.style.NormalTheme
        ThemeManager.Theme.Private -> R.style.PrivateTheme
    }

    setTheme(themeCode)
}

class DefaultThemeManager(
    private var _currentTheme: ThemeManager.Theme,
    private val onThemeChanged: (ThemeManager.Theme) -> Unit
) : ThemeManager {
    override val currentTheme: ThemeManager.Theme
        get() = _currentTheme

    override fun setTheme(theme: ThemeManager.Theme) {
        if (theme == _currentTheme) return
        _currentTheme = theme
        onThemeChanged(theme)
    }
}

class CustomTabThemeManager : ThemeManager {
    override val currentTheme = ThemeManager.Theme.Normal
    override fun setTheme(theme: ThemeManager.Theme) { /* noop */ }
}
