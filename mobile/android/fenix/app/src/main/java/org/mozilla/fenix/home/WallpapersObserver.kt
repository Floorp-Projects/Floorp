/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.graphics.Bitmap
import android.widget.ImageView
import androidx.annotation.VisibleForTesting
import androidx.annotation.WorkerThread
import androidx.core.view.isVisible
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.LifecycleOwner
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.lib.state.Store
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.ext.scaleToBottomOfView
import org.mozilla.fenix.utils.Settings
import org.mozilla.fenix.wallpapers.Wallpaper
import org.mozilla.fenix.wallpapers.WallpapersUseCases

/**
 * [LifecycleObserver] that will immediately start observing the store for wallpapers updates
 * to apply them to the passed in [wallpaperImageView] and automatically stop observing for updates
 * when the [LifecycleOwner] is destroyed.
 *
 * @param appStore Holds the details abut the current wallpaper.
 * @param settings Used for checking user's option for what wallpaper to use.
 * @param wallpapersUseCases Used for interacting with the wallpaper feature.
 * @param wallpaperImageView Serves as the target when applying wallpapers.
 * @param backgroundWorkDispatcher Used for scheduling the wallpaper update when the state is updated
 * with a new wallpaper.
 */
class WallpapersObserver(
    private val appStore: AppStore,
    private val settings: Settings,
    private val wallpapersUseCases: WallpapersUseCases,
    private val wallpaperImageView: ImageView,
    backgroundWorkDispatcher: CoroutineDispatcher = Dispatchers.IO,
) : DefaultLifecycleObserver {
    @VisibleForTesting
    internal var observeWallpapersStoreSubscription: Store.Subscription<AppState, AppAction>? = null

    /**
     * Coroutine scope for updating the wallpapers when an update is observed.
     * Allows for easy cleanup when the client of this is destroyed.
     */
    @VisibleForTesting
    internal var wallpapersScope = CoroutineScope(backgroundWorkDispatcher)

    /**
     * Setting the wallpaper assumes two steps:
     * - load - running on IO
     * - set - running on Main
     * This property caches the result of [loadWallpaper] to be later used by [applyCurrentWallpaper].
     */
    @Volatile
    @VisibleForTesting
    internal var currentWallpaperImage: Bitmap? = null

    /**
     * Listener for when the first observed wallpaper is loaded and available to be set.
     */

    @VisibleForTesting
    internal val isWallpaperLoaded = CompletableDeferred<Unit>()

    init {
        observeWallpaperUpdates()
    }

    /**
     * Immediately apply the current wallpaper automatically adjusted to support
     * the current configuration - portrait or landscape.
     */
    internal suspend fun applyCurrentWallpaper() {
        isWallpaperLoaded.await()

        withContext(Dispatchers.Main.immediate) {
            with(currentWallpaperImage) {
                when (this) {
                    null -> wallpaperImageView.isVisible = false
                    else -> {
                        scaleToBottomOfView(wallpaperImageView)
                        wallpaperImageView.isVisible = true
                    }
                }
            }
        }
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

            // Use the persisted wallpaper name to wait until a state update
            // that contains the wallpaper that the user chose.
            // Avoids setting the AppState default wallpaper if we know that another wallpaper is chosen.
            if (currentValue.name != settings.currentWallpaperName) {
                return@observeManually
            }

            // Use the wallpaper name to differentiate between updates to properly support
            // the restored from settings wallpaper being the same as the one downloaded
            // case in which details like "collection" may be different.
            // Avoids setting the same wallpaper twice.
            if (currentValue.name != lastObservedValue?.name) {
                lastObservedValue = currentValue

                wallpapersScope.launch {
                    loadWallpaper(currentValue)
                    applyCurrentWallpaper()
                }
            }
        }.also {
            it.resume()
        }
    }

    /**
     * Load the bitmap of [wallpaper] and cache it in [currentWallpaperImage].
     */
    @WorkerThread
    @VisibleForTesting
    internal suspend fun loadWallpaper(wallpaper: Wallpaper) {
        currentWallpaperImage = when (wallpaper) {
            Wallpaper.Default -> null
            else -> wallpapersUseCases.loadBitmap(wallpaper)
        }

        isWallpaperLoaded.complete(Unit)
    }
}
