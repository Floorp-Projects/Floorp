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
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.R
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.ext.isSystemInDarkTheme
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
 *
 * @property initialize Usecase for initializing wallpaper feature. Should usually be called early
 * in the app's lifetime to ensure that any potential long-running tasks can complete quickly.
 * @property loadBitmap Usecase for loading specific wallpaper bitmaps.
 * @property selectWallpaper Usecase for selecting a new wallpaper.
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
    val initialize: InitializeWallpapersUseCase by lazy {
        if (FeatureFlags.wallpaperV2Enabled) {
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
        } else {
            val fileManager = LegacyWallpaperFileManager(storageRootDirectory)
            val downloader = LegacyWallpaperDownloader(context, client)
            LegacyInitializeWallpaperUseCase(
                appStore = appStore,
                downloader = downloader,
                fileManager = fileManager,
                settings = context.settings(),
                currentLocale = currentLocale,
            )
        }
    }
    val loadBitmap: LoadBitmapUseCase by lazy {
        if (FeatureFlags.wallpaperV2Enabled) {
            DefaultLoadBitmapUseCase(
                filesDir = context.filesDir,
                getOrientation = { context.resources.configuration.orientation },
            )
        } else {
            LegacyLoadBitmapUseCase(context)
        }
    }
    val loadThumbnail: LoadThumbnailUseCase by lazy {
        if (FeatureFlags.wallpaperV2Enabled) {
            DefaultLoadThumbnailUseCase(storageRootDirectory)
        } else {
            LegacyLoadThumbnailUseCase(context)
        }
    }
    val selectWallpaper: SelectWallpaperUseCase by lazy {
        if (FeatureFlags.wallpaperV2Enabled) {
            DefaultSelectWallpaperUseCase(context.settings(), appStore, fileManager, downloader)
        } else {
            LegacySelectWallpaperUseCase(context.settings(), appStore)
        }
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

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal class LegacyInitializeWallpaperUseCase(
        private val appStore: AppStore,
        private val downloader: LegacyWallpaperDownloader,
        private val fileManager: LegacyWallpaperFileManager,
        private val settings: Settings,
        private val currentLocale: String,
        private val possibleWallpapers: List<Wallpaper> = allWallpapers,
    ) : InitializeWallpapersUseCase {

        /**
         * Downloads the currently available wallpaper metadata from a remote source.
         * Updates the [appStore] with that metadata and with the selected wallpaper found in storage.
         * Removes any unused promotional or time-limited assets from local storage.
         * Should usually be called early the app's lifetime to ensure that metadata and thumbnails
         * are available as soon as they are needed.
         */
        override suspend operator fun invoke() {
            // Quite a bit of code needs to be executed off the main thread in some of this setup.
            // This should be cleaned up as improvements are made to the storage, file management,
            // and download utilities.
            withContext(Dispatchers.IO) {
                val dispatchedCurrent = Wallpaper.getCurrentWallpaperFromSettings(settings)?.let {
                    // Dispatch this ASAP so the home screen can render.
                    appStore.dispatch(AppAction.WallpaperAction.UpdateCurrentWallpaper(it))
                    true
                } ?: false
                val availableWallpapers = possibleWallpapers.getAvailableWallpapers()
                val currentWallpaperName = settings.currentWallpaperName
                val currentWallpaper = possibleWallpapers.find { it.name == currentWallpaperName }
                    ?: fileManager.lookupExpiredWallpaper(currentWallpaperName)
                    ?: Wallpaper.Default

                fileManager.clean(
                    currentWallpaper,
                    possibleWallpapers,
                )
                downloadAllRemoteWallpapers(availableWallpapers)
                appStore.dispatch(AppAction.WallpaperAction.UpdateAvailableWallpapers(availableWallpapers))
                if (!dispatchedCurrent) {
                    appStore.dispatch(AppAction.WallpaperAction.UpdateCurrentWallpaper(currentWallpaper))
                }
            }
        }

        private fun List<Wallpaper>.getAvailableWallpapers() =
            this.filter { !it.isExpired() && it.isAvailableInLocale() }

        private suspend fun downloadAllRemoteWallpapers(availableWallpapers: List<Wallpaper>) {
            for (wallpaper in availableWallpapers) {
                if (wallpaper != Wallpaper.Default) {
                    downloader.downloadWallpaper(wallpaper)
                }
            }
        }

        private fun Wallpaper.isExpired(): Boolean {
            val expired = this.collection.endDate?.let { Date().after(it) } ?: false
            return expired && this.name != settings.currentWallpaperName
        }

        private fun Wallpaper.isAvailableInLocale(): Boolean =
            this.collection.availableLocales?.contains(currentLocale) ?: true

        companion object {
            private val firefoxClassicCollection = Wallpaper.Collection(
                name = Wallpaper.firefoxCollectionName,
                heading = null,
                description = null,
                availableLocales = null,
                startDate = null,
                endDate = null,
                learnMoreUrl = null,
            )
            private val localWallpapers: List<Wallpaper> = listOf(
                Wallpaper(
                    name = Wallpaper.amethystName,
                    collection = firefoxClassicCollection,
                    textColor = null,
                    cardColorLight = null,
                    cardColorDark = null,
                    thumbnailFileState = Wallpaper.ImageFileState.Unavailable,
                    assetsFileState = Wallpaper.ImageFileState.Downloaded,
                ),
                Wallpaper(
                    name = Wallpaper.ceruleanName,
                    collection = firefoxClassicCollection,
                    textColor = null,
                    cardColorLight = null,
                    cardColorDark = null,
                    thumbnailFileState = Wallpaper.ImageFileState.Unavailable,
                    assetsFileState = Wallpaper.ImageFileState.Downloaded,
                ),
                Wallpaper(
                    name = Wallpaper.sunriseName,
                    collection = firefoxClassicCollection,
                    textColor = null,
                    cardColorLight = null,
                    cardColorDark = null,
                    thumbnailFileState = Wallpaper.ImageFileState.Unavailable,
                    assetsFileState = Wallpaper.ImageFileState.Downloaded,
                ),
            )
            private val remoteWallpapers: List<Wallpaper> = listOf(
                Wallpaper(
                    name = Wallpaper.twilightHillsName,
                    collection = firefoxClassicCollection,
                    textColor = null,
                    cardColorLight = null,
                    cardColorDark = null,
                    thumbnailFileState = Wallpaper.ImageFileState.Unavailable,
                    assetsFileState = Wallpaper.ImageFileState.Downloaded,
                ),
                Wallpaper(
                    name = Wallpaper.beachVibeName,
                    collection = firefoxClassicCollection,
                    textColor = null,
                    cardColorLight = null,
                    cardColorDark = null,
                    thumbnailFileState = Wallpaper.ImageFileState.Unavailable,
                    assetsFileState = Wallpaper.ImageFileState.Downloaded,
                ),
            )
            val allWallpapers = listOf(Wallpaper.Default) + localWallpapers + remoteWallpapers
        }
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
    internal class LegacyLoadBitmapUseCase(private val context: Context) : LoadBitmapUseCase {
        /**
         * Load the bitmap for a [wallpaper], if available.
         *
         * @param wallpaper The wallpaper to load a bitmap for.
         */
        override suspend operator fun invoke(wallpaper: Wallpaper): Bitmap? = when (wallpaper.name) {
            Wallpaper.amethystName, Wallpaper.ceruleanName, Wallpaper.sunriseName -> {
                loadWallpaperFromDrawable(context, wallpaper)
            }
            Wallpaper.twilightHillsName, Wallpaper.beachVibeName -> {
                loadWallpaperFromDisk(context, wallpaper)
            }
            else -> null
        }

        private suspend fun loadWallpaperFromDrawable(
            context: Context,
            wallpaper: Wallpaper,
        ): Bitmap? = Result.runCatching {
            val drawableRes = when (wallpaper.name) {
                Wallpaper.amethystName -> R.drawable.amethyst
                Wallpaper.ceruleanName -> R.drawable.cerulean
                Wallpaper.sunriseName -> R.drawable.sunrise
                else -> return@runCatching null
            }
            withContext(Dispatchers.IO) {
                BitmapFactory.decodeResource(context.resources, drawableRes)
            }
        }.getOrNull()

        private suspend fun loadWallpaperFromDisk(
            context: Context,
            wallpaper: Wallpaper,
        ): Bitmap? = Result.runCatching {
            val path = wallpaper.getLocalPathFromContext(context)
            withContext(Dispatchers.IO) {
                val file = File(context.filesDir, path)
                BitmapFactory.decodeStream(file.inputStream())
            }
        }.getOrNull()

        /**
         * Get the expected local path on disk for a wallpaper. This will differ depending
         * on orientation and app theme.
         */
        private fun Wallpaper.getLocalPathFromContext(context: Context): String {
            val orientation = if (context.isLandscape()) "landscape" else "portrait"
            val theme = if (context.isSystemInDarkTheme()) "dark" else "light"
            return Wallpaper.legacyGetLocalPath(orientation, theme, name)
        }

        private fun Context.isLandscape(): Boolean {
            return resources.configuration.orientation == Configuration.ORIENTATION_LANDSCAPE
        }
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
    internal class LegacyLoadThumbnailUseCase(private val context: Context) : LoadThumbnailUseCase {
        override suspend fun invoke(wallpaper: Wallpaper): Bitmap? =
            LegacyLoadBitmapUseCase(context).invoke(wallpaper)
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
    internal class LegacySelectWallpaperUseCase(
        private val settings: Settings,
        private val appStore: AppStore,
    ) : SelectWallpaperUseCase {
        /**
         * Select a new wallpaper. Storage and the app store will be updated appropriately.
         *
         * @param wallpaper The selected wallpaper.
         */
        override suspend fun invoke(wallpaper: Wallpaper): Wallpaper.ImageFileState {
            settings.currentWallpaperName = wallpaper.name
            settings.currentWallpaperTextColor = wallpaper.textColor ?: 0
            settings.currentWallpaperCardColorLight = wallpaper.cardColorLight ?: 0
            settings.currentWallpaperCardColorDark = wallpaper.cardColorDark ?: 0
            appStore.dispatch(AppAction.WallpaperAction.UpdateCurrentWallpaper(wallpaper))
            return Wallpaper.ImageFileState.Downloaded
        }
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
