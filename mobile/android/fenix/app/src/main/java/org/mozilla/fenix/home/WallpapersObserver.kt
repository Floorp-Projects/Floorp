/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.widget.ImageView
import androidx.annotation.VisibleForTesting
import androidx.core.view.isVisible
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.LifecycleOwner
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import mozilla.components.lib.state.Store
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.ext.scaleToBottomOfView
import org.mozilla.fenix.wallpapers.Wallpaper
import org.mozilla.fenix.wallpapers.WallpapersUseCases

/**
 * [LifecycleObserver] that will immediately start observing the store for wallpapers updates
 * to apply them to the passed in [wallpaperImageView] and automatically stop observing for updates
 * when the [LifecycleOwner] is destroyed.
 *
 * @param appStore Holds the details abut the current wallpaper.
 * @param wallpapersUseCases Used for interacting with the wallpaper feature.
 * @param wallpaperImageView Serves as the target when applying wallpapers.
 */
class WallpapersObserver(
    private val appStore: AppStore,
    private val wallpapersUseCases: WallpapersUseCases,
    private val wallpaperImageView: ImageView,
) : DefaultLifecycleObserver {
    @VisibleForTesting
    internal var observeWallpapersStoreSubscription: Store.Subscription<AppState, AppAction>? = null
    @VisibleForTesting
    internal var wallpapersScope = CoroutineScope(Dispatchers.IO)

    init {
        observeWallpaperUpdates()
    }

    /**
     * Immediately apply the current wallpaper automatically adjusted to support
     * the current configuration - portrait or landscape.
     */
    suspend fun applyCurrentWallpaper() {
        showWallpaper()
    }

    override fun onDestroy(owner: LifecycleOwner) {
        observeWallpapersStoreSubscription?.unsubscribe()
        wallpapersScope.cancel()
    }

    @VisibleForTesting
    internal fun observeWallpaperUpdates() {
        var lastObservedValue: Wallpaper? = null
        observeWallpapersStoreSubscription = appStore.observeManually { state ->
            val currentValue = state.wallpaperState.currentWallpaper
            // Use the wallpaper name to differentiate between updates to properly support
            // the restored from settings wallpaper being the same as the one downloaded
            // case in which details like "collection" may be different.
            if (currentValue.name != lastObservedValue?.name) {
                lastObservedValue = currentValue

                wallpapersScope.launch { showWallpaper(currentValue) }
            }
        }.also {
            it.resume()
        }
    }

    @VisibleForTesting
    internal suspend fun showWallpaper(wallpaper: Wallpaper = appStore.state.wallpaperState.currentWallpaper) {
        when (wallpaper) {
            // We only want to update the wallpaper when it's different from the default one
            // as the default is applied already on xml by default.
            Wallpaper.Default -> {
                wallpaperImageView.isVisible = false
            }
            else -> {
                val bitmap = wallpapersUseCases.loadBitmap(wallpaper)
                bitmap?.let {
                    it.scaleToBottomOfView(wallpaperImageView)
                    wallpaperImageView.isVisible = true
                }
            }
        }
    }
}
