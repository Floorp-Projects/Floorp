/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.wallpapers

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
            availableWallpapers = listOf()
        )
    }
}
