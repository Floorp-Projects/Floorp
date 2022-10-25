/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.wallpapers

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Represents all state related to the Wallpapers feature.
 *
 * @property currentWallpaper The currently selected [Wallpaper].
 * @property availableWallpapers The full list of wallpapers that can be selected.
 */
data class WallpaperState(
    val currentWallpaper: Wallpaper,
    val availableWallpapers: List<Wallpaper>,
) {
    companion object {
        val default = WallpaperState(
            currentWallpaper = Wallpaper.Default,
            availableWallpapers = listOf(),
        )
    }

    /**
     * Helper property used to obtain the [Color] to fill-in cards in front of a wallpaper.
     *
     * @return The appropriate light or dark wallpaper card [Color], if available, otherwise a default.
     */
    val wallpaperCardColor: Color
        @Composable get() = when {
            currentWallpaper.cardColorLight != null && currentWallpaper.cardColorDark != null -> {
                if (isSystemInDarkTheme()) {
                    Color(currentWallpaper.cardColorDark)
                } else {
                    Color(currentWallpaper.cardColorLight)
                }
            }
            else -> FirefoxTheme.colors.layer2
        }

    /**
     * Run the Composable [run] block only if the current wallpaper's card colors are available.
     */
    @Composable
    fun composeRunIfWallpaperCardColorsAreAvailable(
        run: @Composable (cardColorLight: Color, cardColorDark: Color) -> Unit,
    ) {
        if (currentWallpaper.cardColorLight != null && currentWallpaper.cardColorDark != null) {
            run(Color(currentWallpaper.cardColorLight), Color(currentWallpaper.cardColorDark))
        }
    }

    /**
     * Run the [run] block only if the current wallpaper's card colors are available.
     */
    fun runIfWallpaperCardColorsAreAvailable(
        run: (cardColorLight: Int, cardColorDark: Int) -> Unit,
    ) {
        if (currentWallpaper.cardColorLight != null && currentWallpaper.cardColorDark != null) {
            run(currentWallpaper.cardColorLight.toInt(), currentWallpaper.cardColorDark.toInt())
        }
    }
}
