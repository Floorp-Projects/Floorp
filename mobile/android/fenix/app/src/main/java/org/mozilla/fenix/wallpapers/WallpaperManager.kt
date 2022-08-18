/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.wallpapers

import mozilla.components.support.base.log.logger.Logger
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.utils.Settings

/**
 * Provides access to available wallpapers and manages their states.
 */
@Suppress("TooManyFunctions")
class WallpaperManager(
    private val appStore: AppStore,
) {
    val logger = Logger("WallpaperManager")

    val wallpapers get() = appStore.state.wallpaperState.availableWallpapers
    val currentWallpaper: Wallpaper get() = appStore.state.wallpaperState.currentWallpaper

    companion object {
        /**
         *  Get whether the default wallpaper should be used.
         */
        fun isDefaultTheCurrentWallpaper(settings: Settings): Boolean = with(settings.currentWallpaperName) {
            return isEmpty() || equals(defaultWallpaper.name)
        }

        val defaultWallpaper = Wallpaper.Default
    }
}
