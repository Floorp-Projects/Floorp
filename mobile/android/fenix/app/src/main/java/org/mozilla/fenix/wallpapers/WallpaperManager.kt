/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.wallpapers

import android.animation.AnimatorSet
import android.animation.ObjectAnimator
import android.content.Context
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

/**
 * Provides access to available wallpapers and manages their states.
 */
@Suppress("TooManyFunctions")
class WallpaperManager(
    private val settings: Settings,
    private val downloader: WallpaperDownloader,
    private val crashReporter: CrashReporter,
) {
    val logger = Logger("WallpaperManager")
    private val remoteWallpapers = listOf(
        Wallpaper(
            "focus",
            themeCollection = WallpaperThemeCollection.FOCUS
        ),
    )
    var availableWallpapers: List<Wallpaper> = localWallpapers + remoteWallpapers
        private set

    var currentWallpaper: Wallpaper = getCurrentWallpaperFromSettings()
        set(value) {
            settings.currentWallpaper = value.name
            field = value
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
        for (wallpaper in remoteWallpapers) {
            downloader.downloadWallpaper(wallpaper)
        }
    }

    /**
     * Returns the next available [Wallpaper], the [currentWallpaper] is the last one then
     * the first available [Wallpaper] will be returned.
     */
    fun switchToNextWallpaper(): Wallpaper {
        val values = availableWallpapers
        val index = values.indexOf(currentWallpaper) + 1

        return if (index >= values.size) {
            values.first()
        } else {
            values[index]
        }
    }

    private fun getCurrentWallpaperFromSettings(): Wallpaper {
        val currentWallpaper = settings.currentWallpaper
        return if (currentWallpaper.isEmpty()) {
            defaultWallpaper
        } else {
            availableWallpapers.find { it.name == currentWallpaper } ?: defaultWallpaper
        }
    }

    /**
     * Load a wallpaper that is saved locally.
     */
    fun loadSavedWallpaper(context: Context, wallpaper: Wallpaper): Bitmap? =
        if (wallpaper.themeCollection.origin == WallpaperOrigin.LOCAL) {
            loadWallpaperFromDrawables(context, wallpaper)
        } else {
            loadWallpaperFromDisk(context, wallpaper)
        }

    private fun loadWallpaperFromDrawables(context: Context, wallpaper: Wallpaper): Bitmap? = Result.runCatching {
        BitmapFactory.decodeResource(context.resources, wallpaper.drawableId)
    }.getOrNull()

    /**
     * Load a wallpaper from app-specific storage.
     */
    private fun loadWallpaperFromDisk(context: Context, wallpaper: Wallpaper): Bitmap? = Result.runCatching {
        val path = wallpaper.getLocalPathFromContext(context)
        runBlockingIncrement {
            withContext(Dispatchers.IO) {
                val file = File(context.filesDir, path)
                BitmapFactory.decodeStream(file.inputStream())
            }
        }
    }.getOrNull()

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
        val defaultWallpaper = Wallpaper(
            name = "default",
            themeCollection = WallpaperThemeCollection.NONE
        )
        val localWallpapers = listOf(
            defaultWallpaper,
            Wallpaper("amethyst", themeCollection = WallpaperThemeCollection.FIREFOX),
            Wallpaper("cerulean", themeCollection = WallpaperThemeCollection.FIREFOX),
            Wallpaper("sunrise", themeCollection = WallpaperThemeCollection.FIREFOX),
        )
        private const val ANIMATION_DELAY_MS = 1500L
    }
}
