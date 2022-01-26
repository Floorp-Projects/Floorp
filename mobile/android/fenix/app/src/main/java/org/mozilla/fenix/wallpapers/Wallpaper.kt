/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.wallpapers

/**
 * A class that represents an available wallpaper and its state.
 * @property name Indicates the name of this wallpaper.
 * @property portraitPath A file path for the portrait version of this wallpaper.
 * @property landscapePath A file path for the landscape version of this wallpaper.
 * @property isDark Indicates if the most predominant color on the wallpaper is dark.
 * @property themeCollection The theme collection this wallpaper belongs to.
 */
data class Wallpaper(
    val name: String,
    val portraitPath: String,
    val landscapePath: String,
    val isDark: Boolean,
    val themeCollection: WallpaperThemeCollection,
)

/**
 * A type hierarchy representing the different theme collections [Wallpaper]s belong to.
 */
sealed class WallpaperThemeCollection {
    object None : WallpaperThemeCollection()
    object Firefox : WallpaperThemeCollection()
}
