/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.theme

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.Stable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.graphics.Color
import mozilla.components.ui.colors.PhotonColors

/**
 * The theme for Mozilla Firefox for Android (Fenix).
 */
@Composable
fun FirefoxTheme(
    darkTheme: Boolean = isSystemInDarkTheme(),
    content: @Composable () -> Unit
) {
    val colors = if (darkTheme) darkColorPalette else lightColorPalette

    ProvideFirefoxColors(colors) {
        MaterialTheme(
            content = content
        )
    }
}

object FirefoxTheme {
    val colors: FirefoxColors
        @Composable
        get() = localFirefoxColors.current
}

private val darkColorPalette = FirefoxColors(
    surface = PhotonColors.DarkGrey50,
    textPrimary = PhotonColors.LightGrey05,
    textSecondary = PhotonColors.LightGrey05,
    dividerLine = PhotonColors.DarkGrey05
)

private val lightColorPalette = FirefoxColors(
    surface = PhotonColors.White,
    textPrimary = PhotonColors.DarkGrey90,
    textSecondary = PhotonColors.DarkGrey05,
    dividerLine = PhotonColors.LightGrey30
)

/**
 * A custom Color Palette for Mozilla Firefox for Android (Fenix).
 */
@Stable
class FirefoxColors(
    surface: Color,
    textPrimary: Color,
    textSecondary: Color,
    dividerLine: Color
) {
    var surface by mutableStateOf(surface)
        private set
    var textPrimary by mutableStateOf(textPrimary)
        private set
    var textSecondary by mutableStateOf(textSecondary)
        private set
    var dividerLine by mutableStateOf(dividerLine)
        private set

    fun update(other: FirefoxColors) {
        surface = other.surface
        textPrimary = other.textPrimary
        textSecondary = other.textSecondary
        dividerLine = other.dividerLine
    }

    fun copy(): FirefoxColors = FirefoxColors(
        surface = surface,
        textPrimary = textPrimary,
        textSecondary = textSecondary,
        dividerLine = dividerLine
    )
}

@Composable
fun ProvideFirefoxColors(
    colors: FirefoxColors,
    content: @Composable () -> Unit
) {
    val colorPalette = remember {
        // Explicitly creating a new object here so we don't mutate the initial [colors]
        // provided, and overwrite the values set in it.
        colors.copy()
    }
    colorPalette.update(colors)
    CompositionLocalProvider(localFirefoxColors provides colorPalette, content = content)
}

private val localFirefoxColors = staticCompositionLocalOf<FirefoxColors> {
    error("No FirefoxColors provided")
}
