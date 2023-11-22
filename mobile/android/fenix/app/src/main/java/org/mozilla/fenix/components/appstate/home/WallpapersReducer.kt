/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.appstate.home

import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.appstate.AppAction.WallpaperAction
import org.mozilla.fenix.components.appstate.AppState

/**
 * Reducer to handle updating [AppState] with the result of an [WallpaperAction].
 */
object WallpapersReducer {

    /**
     * Reducer to handle updating [AppState] with the result of an [WallpaperAction].
     */
    fun reduce(state: AppState, action: WallpaperAction): AppState = when (action) {
        is WallpaperAction.UpdateCurrentWallpaper ->
            state.copy(
                wallpaperState = state.wallpaperState.copy(currentWallpaper = action.wallpaper),
            )
        is WallpaperAction.UpdateAvailableWallpapers ->
            state.copy(
                wallpaperState = state.wallpaperState.copy(availableWallpapers = action.wallpapers),
            )
        is WallpaperAction.UpdateWallpaperDownloadState -> {
            val wallpapers = state.wallpaperState.availableWallpapers.map {
                if (it.name == action.wallpaper.name) {
                    it.copy(assetsFileState = action.imageState)
                } else {
                    it
                }
            }
            val wallpaperState = state.wallpaperState.copy(availableWallpapers = wallpapers)
            state.copy(wallpaperState = wallpaperState)
        }
        is WallpaperAction.OpenToHome -> state.copy(mode = BrowsingMode.Normal)
    }
}
