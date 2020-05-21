/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.thumbnails.storage

import android.content.Context
import android.graphics.Bitmap
import androidx.annotation.WorkerThread
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.async
import mozilla.components.browser.thumbnails.R
import mozilla.components.browser.thumbnails.utils.ThumbnailDiskCache
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.images.DesiredSize
import mozilla.components.support.images.decoder.AndroidImageDecoder
import java.util.concurrent.Executors

private const val MAXIMUM_SCALE_FACTOR = 2.0f

// Number of worker threads we are using internally.
private const val THREADS = 3

internal val sharedDiskCache = ThumbnailDiskCache()

/**
 * Thumbnail storage layer which handles saving and loading the thumbnail from the disk cache.
 */
class ThumbnailStorage(
    private val context: Context,
    jobDispatcher: CoroutineDispatcher = Executors.newFixedThreadPool(THREADS)
        .asCoroutineDispatcher()
) {
    private val decoders = AndroidImageDecoder()
    private val logger = Logger("ThumbnailStorage")
    private val maximumSize =
        context.resources.getDimensionPixelSize(R.dimen.mozac_browser_thumbnails_maximum_size)
    private val scope = CoroutineScope(jobDispatcher)

    /**
     * Asynchronously loads a thumbnail [Bitmap] for the given session ID or url.
     */
    fun loadThumbnail(sessionIdOrUrl: String): Deferred<Bitmap?> = scope.async {
        loadThumbnailInternal(sessionIdOrUrl).also { loadedThumbnail ->
            if (loadedThumbnail != null) {
                logger.debug(
                    "Loaded thumbnail from disk (sessionIdOrUrl = $sessionIdOrUrl, " +
                        "generationId = ${loadedThumbnail.generationId})"
                )
            } else {
                logger.debug("No thumbnail loaded (sessionIdOrUrl = $sessionIdOrUrl)")
            }
        }
    }

    @WorkerThread
    private fun loadThumbnailInternal(sessionIdOrUrl: String): Bitmap? {
        val desiredSize = DesiredSize(
            targetSize = context.resources.getDimensionPixelSize(R.dimen.mozac_browser_thumbnails_size_default),
            maxSize = maximumSize,
            maxScaleFactor = MAXIMUM_SCALE_FACTOR
        )

        val data = sharedDiskCache.getThumbnailData(context, sessionIdOrUrl)

        if (data != null) {
            return decoders.decode(data, desiredSize)
        }

        return null
    }

    /**
     * Stores the given thumbnail [Bitmap] into the disk cache with the provided session ID or url
     * as its key.
     */
    fun saveThumbnail(sessionIdOrUrl: String, bitmap: Bitmap) {
        logger.debug(
            "Saved thumbnail to disk (sessionIdOrUrl = $sessionIdOrUrl, " +
                "generationId = ${bitmap.generationId})"
        )
        sharedDiskCache.putThumbnailBitmap(context, sessionIdOrUrl, bitmap)
    }
}
