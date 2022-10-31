/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads.share

import android.content.Context
import android.webkit.MimeTypeMap
import androidx.annotation.VisibleForTesting
import androidx.annotation.WorkerThread
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.CoroutineExceptionHandler
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.mapNotNull
import kotlinx.coroutines.launch
import kotlinx.coroutines.withTimeout
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.ShareInternetResourceAction
import mozilla.components.browser.state.selector.findTabOrCustomTabOrSelectedTab
import mozilla.components.browser.state.state.content.ShareInternetResourceState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Headers
import mozilla.components.concept.fetch.Headers.Names.CONTENT_TYPE
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.content.shareMedia
import mozilla.components.support.ktx.kotlin.ifNullOrEmpty
import mozilla.components.support.ktx.kotlin.sanitizeURL
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.io.InputStream
import java.net.URLConnection
import kotlin.math.absoluteValue
import kotlin.random.Random

/**
 * Default mime type for base64 images data URLs not containing the media type.
 */
@VisibleForTesting
internal const val DEFAULT_IMAGE_EXTENSION = "jpg"

/**
 * At most time to allow fo the file to be downloaded and the share intent chooser to appear.
 */
private const val OPERATION_TIMEOUT_MILLIS = 1000L

/**
 * At most characters to be sent for subject / message in the share to operation.
 */
@VisibleForTesting
internal const val CHARACTERS_IN_SHARE_TEXT_LIMIT = 200

/**
 * Subdirectory of Context.getCacheDir() where the resources to be shared are stored.
 *
 * Location must be kept in sync with the paths our FileProvider can share from.
 */
@VisibleForTesting
internal var cacheDirName = "mozac_share_cache"

/**
 * [Middleware] implementation for sharing online resources.
 *
 * This will intercept only [ShareInternetResourceAction] [BrowserAction]s.
 *
 * Following which it will transparently
 *  - download internet resources while respecting the private mode related to cookies handling
 *  - temporarily cache the downloaded resources
 *  - automatically open the platform app chooser to share the cached files with other installed Android apps.
 *
 * with a 1 second timeout to ensure a smooth UX.
 *
 * To finish the process in this small timeframe the feature is recommended to be used only for images.
 *
 *  @param context Android context used for various platform interactions
 *  @param httpClient Client used for downloading internet resources
 *  @param store a reference to the application's [BrowserStore]
 * param Coroutine dispatcher used for the cleanup of old cached files. Defaults to IO.
*/
class ShareDownloadFeature(
    private val context: Context,
    private val httpClient: Client,
    private val store: BrowserStore,
    private val tabId: String?,
    cleanupCacheCoroutineDispatcher: CoroutineDispatcher = IO,
) : LifecycleAwareFeature {
    private val logger = Logger("ShareDownloadMiddleware")

    @VisibleForTesting
    internal var scope: CoroutineScope? = null

    override fun start() {
        scope = store.flowScoped { flow ->
            flow.mapNotNull { state -> state.findTabOrCustomTabOrSelectedTab(tabId) }
                .ifChanged { it.content.share }
                .collect { state ->
                    state.content.share?.let { shareState ->
                        logger.debug("Starting the sharing process")
                        startSharing(shareState)

                        // This is a fire and forget action, not something that we want lingering the tab state.
                        store.dispatch(ShareInternetResourceAction.ConsumeShareAction(state.id))
                    }
                }
        }
    }

    override fun stop() {
        scope?.cancel()
    }

    init {
        CoroutineScope(cleanupCacheCoroutineDispatcher).launch {
            cleanupCache()
        }
    }

    @VisibleForTesting
    internal fun startSharing(internetResource: ShareInternetResourceState) {
        val coroutineExceptionHandler = CoroutineExceptionHandler { _, throwable ->
            when (throwable) {
                is InterruptedException -> {
                    logger.warn("Share failed: operation timeout reached")
                }
                is IOException,
                is RuntimeException,
                is NullPointerException,
                -> {
                    logger.warn("Share failed: $throwable")
                }
            }
        }

        scope?.launch(coroutineExceptionHandler) {
            withTimeout(OPERATION_TIMEOUT_MILLIS) {
                val download = download(internetResource)
                share(
                    contentType = internetResource.contentType,
                    filePath = download.canonicalPath,
                    message = sanitizeLongUrls(internetResource.url),
                )
            }
        }
    }

    @WorkerThread
    @VisibleForTesting
    internal fun download(internetResource: ShareInternetResourceState): File {
        val request = Request(internetResource.url.sanitizeURL(), private = internetResource.private)
        val response = if (internetResource.response == null) {
            httpClient.fetch(request)
        } else {
            requireNotNull(internetResource.response)
        }

        if (response.status != Response.SUCCESS) {
            // We experienced a problem trying to fetch the file, nothing more we can do.
            throw(RuntimeException("Resource is not available to download"))
        }

        var tempFile: File? = null
        response.body.useStream { input ->
            val fileExtension = '.' + getFileExtension(response.headers, input)
            tempFile = getTempFile(fileExtension)
            FileOutputStream(tempFile).use { output -> input.copyTo(output) }
        }

        return tempFile!!
    }

    @VisibleForTesting
    internal fun share(
        filePath: String,
        contentType: String?,
        subject: String? = null,
        message: String? = null,
    ) = context.shareMedia(filePath, contentType, subject, message)

    @VisibleForTesting
    internal fun getFilename(fileExtension: String) =
        Random.nextInt().absoluteValue.toString() + fileExtension

    @VisibleForTesting
    internal fun getTempFile(fileExtension: String) =
        File(getMediaShareCacheDirectory(), getFilename(fileExtension))

    @VisibleForTesting
    internal fun getCacheDirectory() = File(context.cacheDir, cacheDirName)

    @VisibleForTesting
    internal fun getFileExtension(responseHeaders: Headers, responseStream: InputStream): String {
        val mimeType = URLConnection.guessContentTypeFromStream(responseStream) ?: responseHeaders[CONTENT_TYPE]

        return MimeTypeMap.getSingleton().getExtensionFromMimeType(mimeType).ifNullOrEmpty { DEFAULT_IMAGE_EXTENSION }
    }

    @VisibleForTesting
    internal fun getMediaShareCacheDirectory(): File {
        val mediaShareCacheDir = getCacheDirectory()
        if (!mediaShareCacheDir.exists()) {
            mediaShareCacheDir.mkdirs()
        }
        return mediaShareCacheDir
    }

    @VisibleForTesting
    internal fun cleanupCache() {
        logger.debug("Deleting previous cache of shared files")
        getCacheDirectory().listFiles()?.forEach { it.delete() }
    }

    /**
     * Ensure url is not too long to be cumbersome from a UX standpoint.
     *
     * @param url to check
     * @return new [String]
     *   - with at most [CHARACTERS_IN_SHARE_TEXT_LIMIT] characters for HTTP URLs
     *   - empty for data URLs
     */
    @VisibleForTesting
    internal fun sanitizeLongUrls(url: String): String {
        return if (url.startsWith("data")) {
            ""
        } else {
            url.take(CHARACTERS_IN_SHARE_TEXT_LIMIT)
        }
    }
}
