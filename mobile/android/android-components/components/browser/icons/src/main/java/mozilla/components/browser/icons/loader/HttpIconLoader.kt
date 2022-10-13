/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.loader

import android.content.Context
import android.os.SystemClock
import android.util.LruCache
import androidx.annotation.VisibleForTesting
import androidx.core.net.toUri
import mozilla.components.browser.icons.Icon
import mozilla.components.browser.icons.IconRequest
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.concept.fetch.isSuccess
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.net.isHttpOrHttps
import mozilla.components.support.ktx.kotlin.sanitizeURL
import java.io.IOException
import java.util.concurrent.TimeUnit

private const val CONNECT_TIMEOUT = 2L // Seconds
private const val READ_TIMEOUT = 10L // Seconds

/**
 * [IconLoader] implementation that will try to download the icon for resources that point to an http(s) URL.
 */
open class HttpIconLoader(
    private val httpClient: Client,
) : IconLoader {
    private val logger = Logger("HttpIconLoader")
    private val failureCache = FailureCache()

    override fun load(context: Context, request: IconRequest, resource: IconRequest.Resource): IconLoader.Result {
        if (!shouldDownload(resource)) {
            return IconLoader.Result.NoResult
        }

        // Right now we always perform a download. We shouldn't retry to download from URLs that have failed just
        // recently: https://github.com/mozilla-mobile/android-components/issues/2591

        return internalLoad(request, resource)
    }

    protected fun internalLoad(request: IconRequest, resource: IconRequest.Resource): IconLoader.Result {
        val downloadRequest = Request(
            url = resource.url.sanitizeURL(),
            method = Request.Method.GET,
            cookiePolicy = if (request.isPrivate) {
                Request.CookiePolicy.OMIT
            } else {
                Request.CookiePolicy.INCLUDE
            },
            connectTimeout = Pair(CONNECT_TIMEOUT, TimeUnit.SECONDS),
            readTimeout = Pair(READ_TIMEOUT, TimeUnit.SECONDS),
            redirect = Request.Redirect.FOLLOW,
            useCaches = true,
            private = request.isPrivate,
        )

        return try {
            val response = httpClient.fetch(downloadRequest)
            if (response.isSuccess) {
                response.toIconLoaderResult()
            } else {
                failureCache.rememberFailure(resource.url)
                IconLoader.Result.NoResult
            }
        } catch (e: IOException) {
            logger.debug("IOException while trying to download icon resource", e)
            IconLoader.Result.NoResult
        }
    }

    protected fun shouldDownload(resource: IconRequest.Resource): Boolean {
        return resource.url.sanitizeURL().toUri().isHttpOrHttps && !failureCache.hasFailedRecently(resource.url)
    }
}

private fun Response.toIconLoaderResult() = body.useStream {
    IconLoader.Result.BytesResult(it.readBytes(), Icon.Source.DOWNLOAD)
}

private const val MAX_FAILURE_URLS = 25
private const val FAILURE_RETRY_MILLISECONDS = 1000 * 60 * 30 // 30 Minutes

@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal class FailureCache {
    private val cache = LruCache<String, Long>(MAX_FAILURE_URLS)

    /**
     * Remembers this [url] after loading from it has failed.
     */
    fun rememberFailure(url: String) {
        cache.put(url, now())
    }

    /**
     * Has loading from this [url] failed previously and recently?
     */
    fun hasFailedRecently(url: String) =
        cache.get(url)?.let { failedAt ->
            if (failedAt + FAILURE_RETRY_MILLISECONDS < now()) {
                cache.remove(url)
                false
            } else {
                true
            }
        } ?: false

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun now() = SystemClock.elapsedRealtime()
}
