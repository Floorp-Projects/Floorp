/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.wallpapers

import android.animation.AnimatorSet
import android.animation.ObjectAnimator
import android.content.Context
import android.content.res.Configuration
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.drawable.BitmapDrawable
import android.os.Handler
import android.os.Looper
import android.view.View
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.content.getColorFromAttr
import org.mozilla.fenix.R
import org.mozilla.fenix.perf.runBlockingIncrement
import org.mozilla.fenix.utils.Settings
import java.io.File
import java.util.Date

/**
 * Provides access to available wallpapers and manages their states.
 */
@Suppress("TooManyFunctions")
class WallpaperManager(
    private val settings: Settings,
    private val downloader: WallpaperDownloader,
    private val fileManager: WallpaperFileManager,
    private val crashReporter: CrashReporter,
    allWallpapers: List<Wallpaper> = availableWallpapers
) {
    val logger = Logger("WallpaperManager")

    val wallpapers = allWallpapers.filter(::filterExpiredRemoteWallpapers)

    var currentWallpaper: Wallpaper = getCurrentWallpaperFromSettings()
        set(value) {
            settings.currentWallpaper = value.name
            field = value
        }

    init {
        fileManager.clean(currentWallpaper, wallpapers.filterIsInstance<Wallpaper.Remote>())
    }

    /**
     * Apply the [newWallpaper] into the [wallpaperContainer] and update the [currentWallpaper].
     */
    fun updateWallpaper(wallpaperContainer: View, newWallpaper: Wallpaper) {
        val context = wallpaperContainer.context
        if (newWallpaper == defaultWallpaper) {
            wallpaperContainer.setBackgroundColor(context.getColorFromAttr(DEFAULT_RESOURCE))
            logger.info("Wallpaper update to default background")
        } else {
            val bitmap = loadSavedWallpaper(context, newWallpaper)
            if (bitmap == null) {
                val message = "Could not load wallpaper bitmap. Resetting to default."
                logger.error(message)
                crashReporter.submitCaughtException(NullPointerException(message))
                wallpaperContainer.setBackgroundColor(context.getColorFromAttr(DEFAULT_RESOURCE))
                currentWallpaper = defaultWallpaper
            } else {
                wallpaperContainer.background = BitmapDrawable(context.resources, bitmap)
            }
        }
        currentWallpaper = newWallpaper
    }

    /**
     * Download all known remote wallpapers.
     */
    suspend fun downloadAllRemoteWallpapers() {
        for (wallpaper in wallpapers.filterIsInstance<Wallpaper.Remote>()) {
            downloader.downloadWallpaper(wallpaper)
        }
    }

    /**
     * Returns the next available [Wallpaper], the [currentWallpaper] is the last one then
     * the first available [Wallpaper] will be returned.
     */
    fun switchToNextWallpaper(): Wallpaper {
        val values = wallpapers
        val index = values.indexOf(currentWallpaper) + 1

        return if (index >= values.size) {
            values.first()
        } else {
            values[index]
        }
    }

    private fun filterExpiredRemoteWallpapers(wallpaper: Wallpaper): Boolean = when (wallpaper) {
        is Wallpaper.Remote -> {
            val notExpired = wallpaper.expirationDate?.let { Date().before(it) } ?: true
            notExpired || wallpaper.name == settings.currentWallpaper
        }
        else -> true
    }

    private fun getCurrentWallpaperFromSettings(): Wallpaper {
        val currentWallpaper = settings.currentWallpaper
        return if (currentWallpaper.isEmpty()) {
            defaultWallpaper
        } else {
            wallpapers.find { it.name == currentWallpaper }
                ?: fileManager.lookupExpiredWallpaper(currentWallpaper)
                ?: defaultWallpaper
        }
    }

    /**
     * Load a wallpaper that is saved locally.
     */
    fun loadSavedWallpaper(context: Context, wallpaper: Wallpaper): Bitmap? =
        when (wallpaper) {
            is Wallpaper.Local -> loadWallpaperFromDrawables(context, wallpaper)
            is Wallpaper.Remote -> loadWallpaperFromDisk(context, wallpaper)
            else -> null
        }

    private fun loadWallpaperFromDrawables(context: Context, wallpaper: Wallpaper.Local): Bitmap? = Result.runCatching {
        BitmapFactory.decodeResource(context.resources, wallpaper.drawableId)
    }.getOrNull()

    /**
     * Load a wallpaper from app-specific storage.
     */
    private fun loadWallpaperFromDisk(context: Context, wallpaper: Wallpaper.Remote): Bitmap? = Result.runCatching {
        val path = wallpaper.getLocalPathFromContext(context)
        runBlockingIncrement {
            withContext(Dispatchers.IO) {
                val file = File(context.filesDir, path)
                BitmapFactory.decodeStream(file.inputStream())
            }
        }
    }.getOrNull()

    /**
     * Get the expected local path on disk for a wallpaper. This will differ depending
     * on orientation and app theme.
     */
    private fun Wallpaper.Remote.getLocalPathFromContext(context: Context): String {
        val orientation = if (context.isLandscape()) "landscape" else "portrait"
        val theme = if (context.isDark()) "dark" else "light"
        return Wallpaper.getBaseLocalPath(orientation, theme, name)
    }

    private fun Context.isLandscape(): Boolean {
        return resources.configuration.orientation == Configuration.ORIENTATION_LANDSCAPE
    }

    private fun Context.isDark(): Boolean {
        return resources.configuration.uiMode and
            Configuration.UI_MODE_NIGHT_MASK == Configuration.UI_MODE_NIGHT_YES
    }

    /**
     * Animates the Firefox logo, if it hasn't been animated before, otherwise nothing will happen.
     * After animating the first time, the [Settings.shouldAnimateFirefoxLogo] setting
     * will be updated.
     */
    @Suppress("MagicNumber")
    fun animateLogoIfNeeded(logo: View) {
        if (!settings.shouldAnimateFirefoxLogo) {
            return
        }
        Handler(Looper.getMainLooper()).postDelayed(
            {
                val animator1 = ObjectAnimator.ofFloat(logo, "rotation", 0f, 10f)
                val animator2 = ObjectAnimator.ofFloat(logo, "rotation", 10f, 0f)
                val animator3 = ObjectAnimator.ofFloat(logo, "rotation", 0f, 10f)
                val animator4 = ObjectAnimator.ofFloat(logo, "rotation", 10f, 0f)

                animator1.duration = 200
                animator2.duration = 200
                animator3.duration = 200
                animator4.duration = 200

                val set = AnimatorSet()

                set.play(animator1).before(animator2).after(animator3).before(animator4)
                set.start()

                settings.shouldAnimateFirefoxLogo = false
            },
            ANIMATION_DELAY_MS
        )
    }

    companion object {
        const val DEFAULT_RESOURCE = R.attr.homeBackground
        val defaultWallpaper = Wallpaper.Default
        private val localWallpapers: List<Wallpaper.Local> = listOf(
            Wallpaper.Local.Firefox("amethyst", R.drawable.amethyst),
            Wallpaper.Local.Firefox("cerulean", R.drawable.cerulean),
            Wallpaper.Local.Firefox("sunrise", R.drawable.sunrise),
        )
        private val remoteWallpapers: List<Wallpaper.Remote> = listOf(
            Wallpaper.Remote.Focus(
                "focus",
            ),
        )
        private val availableWallpapers = listOf(defaultWallpaper) + localWallpapers + remoteWallpapers
        private const val ANIMATION_DELAY_MS = 1500L
    }
}
