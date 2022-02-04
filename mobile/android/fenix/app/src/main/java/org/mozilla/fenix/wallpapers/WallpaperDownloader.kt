/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.wallpapers

import android.content.Context
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.isSuccess
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.support.base.log.logger.Logger
import org.mozilla.fenix.BuildConfig
import java.io.File

/**
 * Can download wallpapers from a remote host.
 *
 * @param context Required for writing files to local storage.
 * @param client Required for fetching files from network.
 */
class WallpaperDownloader(
    private val context: Context,
    private val client: Client,
    private val crashReporter: CrashReporter,
) {
    private val logger = Logger("WallpaperDownloader")
    private val remoteHost = BuildConfig.WALLPAPER_URL

    /**
     * Downloads a wallpaper from the network. Will try to fetch 4 versions of each wallpaper:
     * portrait/light - portrait/dark - landscape/light - landscape/dark. These are expected to be
     * found at a remote path in the form:
     * <WALLPAPER_URL>/<resolution>/<orientation>/<app theme>/<wallpaper theme>/<wallpaper name>.png
     */
    suspend fun downloadWallpaper(wallpaper: Wallpaper.Remote) = withContext(Dispatchers.IO) {
        for (metadata in wallpaper.toMetadata(context)) {
            val localFile = File(context.filesDir.absolutePath, metadata.localPath)
            if (localFile.exists()) continue
            val request = Request(
                url = "$remoteHost/${metadata.remotePath}",
                method = Request.Method.GET
            )
            Result.runCatching {
                val response = client.fetch(request)
                if (!response.isSuccess) {
                    logger.error("Download response failure code: ${response.status}")
                    return@withContext
                }
                File(localFile.path.substringBeforeLast("/")).mkdirs()
                response.body.useStream { input ->
                    input.copyTo(localFile.outputStream())
                }
            }.onFailure {
                Result.runCatching {
                    if (localFile.exists()) {
                        localFile.delete()
                    }
                }.onFailure { e ->
                    logger.error("Failed to delete stale wallpaper bitmaps while downloading", e)
                }

                logger.error(it.message ?: "Download failed: no throwable message included.", it)
                crashReporter.submitCaughtException(it)
            }
        }
    }

    private data class WallpaperMetadata(val remotePath: String, val localPath: String)

    private fun Wallpaper.Remote.toMetadata(context: Context): List<WallpaperMetadata> =
        listOf("landscape", "portrait").flatMap { orientation ->
            listOf("light", "dark").map { theme ->
                val remoteParent = this::class.simpleName!!.lowercase()
                val localPath = "wallpapers/$orientation/$theme/$name.png"
                val remotePath = "${context.resolutionSegment()}/$orientation/$theme/$remoteParent$name.png"
                WallpaperMetadata(remotePath, localPath)
            }
        }

    @Suppress("MagicNumber")
    private fun Context.resolutionSegment(): String = when (resources.displayMetrics.densityDpi) {
        // targeting hdpi and greater density resolutions https://developer.android.com/training/multiscreen/screendensities
        in 0..240 -> "low"
        in 240..320 -> "medium"
        else -> "high"
    }
}
