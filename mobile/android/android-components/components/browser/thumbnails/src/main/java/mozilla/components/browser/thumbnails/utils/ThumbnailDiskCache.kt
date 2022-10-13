/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.thumbnails.utils

import android.content.Context
import android.graphics.Bitmap
import android.os.Build
import androidx.annotation.VisibleForTesting
import com.jakewharton.disklrucache.DiskLruCache
import mozilla.components.concept.base.images.ImageLoadRequest
import mozilla.components.concept.base.images.ImageSaveRequest
import mozilla.components.support.base.log.logger.Logger
import java.io.File
import java.io.IOException

private const val MAXIMUM_CACHE_THUMBNAIL_DATA_BYTES: Long = 1024L * 1024L * 100L // 100 MB
private const val THUMBNAIL_DISK_CACHE_VERSION = 1
private const val WEBP_QUALITY = 90

/**
 * Caching thumbnail bitmaps on disk.
 */
class ThumbnailDiskCache {
    private val logger = Logger("ThumbnailDiskCache")

    @VisibleForTesting
    internal var thumbnailCache: DiskLruCache? = null
    private val thumbnailCacheWriteLock = Any()

    internal fun clear(context: Context) {
        synchronized(thumbnailCacheWriteLock) {
            try {
                getThumbnailCache(context).delete()
            } catch (e: IOException) {
                logger.warn("Thumbnail cache could not be cleared. Perhaps there are none?")
            }
            thumbnailCache = null
        }
    }

    /**
     * Retrieves the thumbnail data from the disk cache for the given session ID or URL.
     *
     * @param context the application [Context].
     * @param request [ImageLoadRequest] providing the session ID or URL of the thumbnail to retrieve.
     * @return the [ByteArray] of the thumbnail or null if the snapshot of the entry does not exist.
     */
    internal fun getThumbnailData(context: Context, request: ImageLoadRequest): ByteArray? {
        val snapshot = getThumbnailCache(context).get(request.id) ?: return null

        return try {
            snapshot.getInputStream(0).use {
                it.buffered().readBytes()
            }
        } catch (e: IOException) {
            logger.info("Failed to read thumbnail bitmap from disk", e)
            null
        }
    }

    /**
     * Stores the given session ID or URL's thumbnail [Bitmap] into the disk cache.
     *
     * @param context the application [Context].
     * @param request [ImageSaveRequest] providing the session ID or URL of the thumbnail to retrieve.
     * @param bitmap the thumbnail [Bitmap] to store.
     */
    internal fun putThumbnailBitmap(context: Context, request: ImageSaveRequest, bitmap: Bitmap) {
        val compressFormat = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            Bitmap.CompressFormat.WEBP_LOSSY
        } else {
            @Suppress("DEPRECATION")
            Bitmap.CompressFormat.WEBP
        }

        try {
            synchronized(thumbnailCacheWriteLock) {
                val editor = getThumbnailCache(context)
                    .edit(request) ?: return

                editor.newOutputStream(0).use { stream ->
                    bitmap.compress(compressFormat, WEBP_QUALITY, stream)
                }

                editor.commit()
            }
        } catch (e: IOException) {
            logger.info("Failed to save thumbnail bitmap to disk", e)
        }
    }

    /**
     * Removes the given session ID or URL's thumbnail [Bitmap] from the disk cache.
     *
     * @param context the application [Context].
     * @param sessionIdOrUrl the session ID or URL.
     */
    internal fun removeThumbnailData(context: Context, sessionIdOrUrl: String) {
        try {
            synchronized(thumbnailCacheWriteLock) {
                getThumbnailCache(context).remove(sessionIdOrUrl)
            }
        } catch (e: IOException) {
            logger.info("Failed to remove thumbnail bitmap from disk", e)
        }
    }

    private fun getThumbnailCacheDirectory(context: Context): File {
        val cacheDirectory = File(context.cacheDir, "mozac_browser_thumbnails")
        return File(cacheDirectory, "thumbnails")
    }

    @Synchronized
    private fun getThumbnailCache(context: Context): DiskLruCache {
        thumbnailCache?.let { return it }

        return DiskLruCache.open(
            getThumbnailCacheDirectory(context),
            THUMBNAIL_DISK_CACHE_VERSION,
            1,
            MAXIMUM_CACHE_THUMBNAIL_DATA_BYTES,
        ).also { thumbnailCache = it }
    }
}
