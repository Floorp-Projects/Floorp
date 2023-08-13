/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.wallpapers

import android.content.Context
import android.content.res.Configuration
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.components.concept.fetch.Client
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.utils.Settings
import java.io.File
import java.util.Date

/**
 * Contains use cases related to the wallpaper feature.
 *
 * @param context Used for various file and configuration checks.
 * @param appStore Will receive dispatches of metadata updates like the currently selected wallpaper.
 * @param client Handles downloading wallpapers and their metadata.
 * @param storageRootDirectory The top level app-local storage directory.
 * @param currentLocale The locale currently being used on the device.
 */
class WallpapersUseCases(
    context: Context,
    appStore: AppStore,
    client: Client,
    storageRootDirectory: File,
    currentLocale: String,
) {
    private val downloader = WallpaperDownloader(storageRootDirectory, client)
    private val fileManager = WallpaperFileManager(storageRootDirectory)

    // Use case for initializing wallpaper feature. Should usually be called early
    // in the app's lifetime to ensure that any potential long-running tasks can complete quickly.
    val initialize: InitializeWallpapersUseCase by lazy {
        val metadataFetcher = WallpaperMetadataFetcher(client)
        val migrationHelper = LegacyWallpaperMigration(
            storageRootDirectory = storageRootDirectory,
            settings = context.settings(),
            selectWallpaper::invoke,
        )
        DefaultInitializeWallpaperUseCase(
            appStore = appStore,
            downloader = downloader,
            fileManager = fileManager,
            metadataFetcher = metadataFetcher,
            migrationHelper = migrationHelper,
            settings = context.settings(),
            currentLocale = currentLocale,
        )
    }

    // Use case for loading specific wallpaper bitmaps.
    val loadBitmap: LoadBitmapUseCase by lazy {
        DefaultLoadBitmapUseCase(
            filesDir = context.filesDir,
            getOrientation = { context.resources.configuration.orientation },
        )
    }

    val loadThumbnail: LoadThumbnailUseCase by lazy {
        DefaultLoadThumbnailUseCase(storageRootDirectory)
    }

    // Use case for selecting a new wallpaper.
    val selectWallpaper: SelectWallpaperUseCase by lazy {
        DefaultSelectWallpaperUseCase(context.settings(), appStore, fileManager, downloader)
    }

    /**
     * Contract for usecases that initialize the wallpaper feature.
     */
    interface InitializeWallpapersUseCase {
        /**
         * Start operations that should be down during initialization, like remote metadata
         * retrieval and determining the currently selected wallpaper.
         */
        suspend operator fun invoke()
    }

    @Suppress("LongParameterList")
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal class DefaultInitializeWallpaperUseCase(
        private val appStore: AppStore,
        private val downloader: WallpaperDownloader,
        private val fileManager: WallpaperFileManager,
        private val metadataFetcher: WallpaperMetadataFetcher,
        private val migrationHelper: LegacyWallpaperMigration,
        private val settings: Settings,
        private val currentLocale: String,
    ) : InitializeWallpapersUseCase {
        override suspend fun invoke() {
            Wallpaper.getCurrentWallpaperFromSettings(settings)?.let {
                appStore.dispatch(AppAction.WallpaperAction.UpdateCurrentWallpaper(it))
            }

            val currentWallpaperName = if (settings.shouldMigrateLegacyWallpaper) {
                val migratedWallpaperName =
                    migrationHelper.migrateLegacyWallpaper(settings.currentWallpaperName)
                settings.currentWallpaperName = migratedWallpaperName
                settings.shouldMigrateLegacyWallpaper = false
                migratedWallpaperName
            } else {
                settings.currentWallpaperName
            }

            if (settings.shouldMigrateLegacyWallpaperCardColors) {
                migrationHelper.migrateExpiredWallpaperCardColors()
            }

            val possibleWallpapers = metadataFetcher.downloadWallpaperList().filter {
                !it.isExpired() && it.isAvailableInLocale()
            }
            val currentWallpaper = possibleWallpapers.find { it.name == currentWallpaperName }
                ?: fileManager.lookupExpiredWallpaper(settings)
                ?: Wallpaper.Default

            // Dispatching this early will make it accessible to the home screen ASAP. If it has been
            // dispatched above, we may still need to update other metadata about it.
            appStore.dispatch(AppAction.WallpaperAction.UpdateCurrentWallpaper(currentWallpaper))

            fileManager.clean(
                currentWallpaper,
                possibleWallpapers,
            )

            val wallpapersWithUpdatedThumbnailState = possibleWallpapers.map { wallpaper ->
                val result = downloader.downloadThumbnail(wallpaper)
                wallpaper.copy(thumbnailFileState = result)
            }

            val defaultIncluded = listOf(Wallpaper.Default) + wallpapersWithUpdatedThumbnailState
            appStore.dispatch(AppAction.WallpaperAction.UpdateAvailableWallpapers(defaultIncluded))
        }

        private fun Wallpaper.isExpired(): Boolean = when (this) {
            Wallpaper.Default -> false
            else -> {
                val expired = this.collection.endDate?.let { Date().after(it) } ?: false
                expired && this.name != settings.currentWallpaperName
            }
        }

        private fun Wallpaper.isAvailableInLocale(): Boolean =
            this.collection.availableLocales?.contains(currentLocale) ?: true
    }

    /**
     * Contract for usecase for loading bitmaps related to a specific wallpaper.
     */
    interface LoadBitmapUseCase {
        /**
         * Load the bitmap for a [wallpaper], if available.
         *
         * @param wallpaper The wallpaper to load a bitmap for.
         */
        suspend operator fun invoke(wallpaper: Wallpaper): Bitmap?
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal class DefaultLoadBitmapUseCase(
        private val filesDir: File,
        private val getOrientation: () -> Int,
    ) : LoadBitmapUseCase {
        override suspend fun invoke(wallpaper: Wallpaper): Bitmap? =
            loadWallpaperFromDisk(wallpaper)

        private suspend fun loadWallpaperFromDisk(
            wallpaper: Wallpaper,
        ): Bitmap? = Result.runCatching {
            val path = wallpaper.getLocalPathFromContext()
            withContext(Dispatchers.IO) {
                val file = File(filesDir, path)
                BitmapFactory.decodeStream(file.inputStream())
            }
        }.getOrNull()

        /**
         * Get the expected local path on disk for a wallpaper. This will differ depending
         * on orientation and app theme.
         */
        private fun Wallpaper.getLocalPathFromContext(): String {
            val orientation = if (isLandscape()) {
                Wallpaper.ImageType.Landscape
            } else {
                Wallpaper.ImageType.Portrait
            }
            return Wallpaper.getLocalPath(name, orientation)
        }

        private fun isLandscape(): Boolean {
            return getOrientation() == Configuration.ORIENTATION_LANDSCAPE
        }
    }

    /**
     * Contract for usecase for loading thumbnail bitmaps related to a specific wallpaper.
     */
    interface LoadThumbnailUseCase {
        /**
         * Load the bitmap for a [wallpaper] thumbnail, if available.
         *
         * @param wallpaper The wallpaper to load a thumbnail for.
         */
        suspend operator fun invoke(wallpaper: Wallpaper): Bitmap?
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal class DefaultLoadThumbnailUseCase(private val filesDir: File) : LoadThumbnailUseCase {
        override suspend fun invoke(wallpaper: Wallpaper): Bitmap? = withContext(Dispatchers.IO) {
            Result.runCatching {
                val path = Wallpaper.getLocalPath(wallpaper.name, Wallpaper.ImageType.Thumbnail)
                withContext(Dispatchers.IO) {
                    val file = File(filesDir, path)
                    BitmapFactory.decodeStream(file.inputStream())
                }
            }.getOrNull()
        }
    }

    /**
     * Contract for usecase of selecting a new wallpaper.
     */
    interface SelectWallpaperUseCase {
        /**
         * Select a new wallpaper.
         *
         * @param wallpaper The selected wallpaper.
         */
        suspend operator fun invoke(wallpaper: Wallpaper): Wallpaper.ImageFileState
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal class DefaultSelectWallpaperUseCase(
        private val settings: Settings,
        private val appStore: AppStore,
        private val fileManager: WallpaperFileManager,
        private val downloader: WallpaperDownloader,
    ) : SelectWallpaperUseCase {
        /**
         * Select a new wallpaper. Storage and the app store will be updated appropriately.
         *
         * @param wallpaper The selected wallpaper.
         */
        override suspend fun invoke(wallpaper: Wallpaper): Wallpaper.ImageFileState {
            return if (wallpaper == Wallpaper.Default || fileManager.wallpaperImagesExist(wallpaper)) {
                selectWallpaper(wallpaper)
                dispatchDownloadState(wallpaper, Wallpaper.ImageFileState.Downloaded)
                Wallpaper.ImageFileState.Downloaded
            } else {
                dispatchDownloadState(wallpaper, Wallpaper.ImageFileState.Downloading)
                val result = downloader.downloadWallpaper(wallpaper)
                dispatchDownloadState(wallpaper, result)
                if (result == Wallpaper.ImageFileState.Downloaded) {
                    selectWallpaper(wallpaper)
                }
                result
            }
        }

        @VisibleForTesting
        internal fun selectWallpaper(wallpaper: Wallpaper) {
            settings.currentWallpaperName = wallpaper.name
            settings.currentWallpaperTextColor = wallpaper.textColor ?: 0L
            settings.currentWallpaperCardColorLight = wallpaper.cardColorLight ?: 0L
            settings.currentWallpaperCardColorDark = wallpaper.cardColorDark ?: 0L
            appStore.dispatch(AppAction.WallpaperAction.UpdateCurrentWallpaper(wallpaper))
        }

        private fun dispatchDownloadState(wallpaper: Wallpaper, downloadState: Wallpaper.ImageFileState) {
            appStore.dispatch(AppAction.WallpaperAction.UpdateWallpaperDownloadState(wallpaper, downloadState))
        }
    }
}
