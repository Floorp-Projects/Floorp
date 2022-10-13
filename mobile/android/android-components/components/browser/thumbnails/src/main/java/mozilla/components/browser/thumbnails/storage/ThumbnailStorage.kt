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
import kotlinx.coroutines.Job
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.async
import kotlinx.coroutines.launch
import mozilla.components.browser.thumbnails.R
import mozilla.components.browser.thumbnails.utils.ThumbnailDiskCache
import mozilla.components.concept.base.images.ImageLoadRequest
import mozilla.components.concept.base.images.ImageSaveRequest
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.utils.NamedThreadFactory
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
    jobDispatcher: CoroutineDispatcher = Executors.newFixedThreadPool(
        THREADS,
        NamedThreadFactory("ThumbnailStorage"),
    ).asCoroutineDispatcher(),
) {
    private val decoders = AndroidImageDecoder()
    private val logger = Logger("ThumbnailStorage")
    private val maximumSize =
        context.resources.getDimensionPixelSize(R.dimen.mozac_browser_thumbnails_maximum_size)
    private val scope = CoroutineScope(jobDispatcher)

    /**
     * Clears all the stored thumbnails in the disk cache.
     */
    fun clearThumbnails(): Job =
        scope.launch {
            logger.debug("Cleared all thumbnails from disk")
            sharedDiskCache.clear(context)
        }

    /**
     * Deletes the given thumbnail [Bitmap] from the disk cache with the provided session ID or url
     * as its key.
     */
    fun deleteThumbnail(sessionIdOrUrl: String): Job =
        scope.launch {
            logger.debug("Removed thumbnail from disk (sessionIdOrUrl = $sessionIdOrUrl)")
            sharedDiskCache.removeThumbnailData(context, sessionIdOrUrl)
        }

    /**
     * Asynchronously loads a thumbnail [Bitmap] for the given [ImageLoadRequest].
     */
    fun loadThumbnail(request: ImageLoadRequest): Deferred<Bitmap?> = scope.async {
        loadThumbnailInternal(request).also { loadedThumbnail ->
            if (loadedThumbnail != null) {
                logger.debug(
                    "Loaded thumbnail from disk (id = ${request.id}, " +
                        "generationId = ${loadedThumbnail.generationId})",
                )
            } else {
                logger.debug("No thumbnail loaded (id = ${request.id})")
            }
        }
    }

    @WorkerThread
    private fun loadThumbnailInternal(request: ImageLoadRequest): Bitmap? {
        val desiredSize = DesiredSize(
            targetSize = request.size,
            minSize = request.size,
            maxSize = maximumSize,
            maxScaleFactor = MAXIMUM_SCALE_FACTOR,
        )

        val data = sharedDiskCache.getThumbnailData(context, request)

        if (data != null) {
            return decoders.decode(data, desiredSize)
        }

        return null
    }

    /**
     * Stores the given thumbnail [Bitmap] into the disk cache with the provided [ImageLoadRequest]
     * as its key.
     */
    fun saveThumbnail(request: ImageSaveRequest, bitmap: Bitmap): Job =
        scope.launch {
            logger.debug(
                "Saved thumbnail to disk (id = $request, " +
                    "generationId = ${bitmap.generationId})",
            )
            sharedDiskCache.putThumbnailBitmap(context, request, bitmap)
        }
}
