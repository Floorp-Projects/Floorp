/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.wallpapers

import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.isSuccess
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.wallpapers.Wallpaper.Companion.getLocalPath
import java.io.File

/**
 * Can download wallpapers from a remote host.
 *
 * @param storageRootDirectory The top level app-local storage directory.
 * @param client Required for fetching files from network.
 * @param dispatcher Dispatcher used to execute suspending functions. Default parameter
 * should be likely be used except for when under test.
 */
class WallpaperDownloader(
    private val storageRootDirectory: File,
    private val client: Client,
    private val dispatcher: CoroutineDispatcher = Dispatchers.IO
) {
    private val remoteHost = BuildConfig.WALLPAPER_URL

    /**
     * Downloads a wallpaper from the network. Will try to fetch 2 versions of each wallpaper:
     * portrait and landscape. These are expected to be found at a remote path in the form:
     * <WALLPAPER_URL>/<collection name>/<wallpaper name>/<orientation>.png
     * and will be stored in the local path:
     * wallpapers/<wallpaper name>/<orientation>.png
     */
    suspend fun downloadWallpaper(wallpaper: Wallpaper) = withContext(dispatcher) {
        for (metadata in wallpaper.toMetadata()) {
            val localFile = File(storageRootDirectory.absolutePath, metadata.localPath)
            // Don't overwrite an asset if it exists
            if (localFile.exists()) continue
            val request = Request(
                url = "$remoteHost/${metadata.remotePath}",
                method = Request.Method.GET
            )
            Result.runCatching {
                val response = client.fetch(request)
                if (!response.isSuccess) {
                    return@withContext
                }
                File(localFile.path.substringBeforeLast("/")).mkdirs()
                response.body.useStream { input ->
                    input.copyTo(localFile.outputStream())
                }
            }.onFailure {
                // This should clean up any partial downloads
                Result.runCatching {
                    if (localFile.exists()) {
                        localFile.delete()
                    }
                }
            }
        }
    }

    private data class WallpaperMetadata(val remotePath: String, val localPath: String)

    private fun Wallpaper.toMetadata(): List<WallpaperMetadata> =
        listOf(Wallpaper.ImageType.Portrait, Wallpaper.ImageType.Landscape).map { orientation ->
            val localPath = getLocalPath(this.name, orientation)
            val remotePath = "${collection.name}/${this.name}/${orientation.lowercase()}.png"
            WallpaperMetadata(remotePath, localPath)
        }
}
