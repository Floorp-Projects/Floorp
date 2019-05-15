/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.loader

import android.content.Context
import mozilla.components.browser.icons.Icon
import mozilla.components.browser.icons.IconRequest
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.isSuccess
import mozilla.components.support.ktx.android.net.isHttpOrHttps
import mozilla.components.support.ktx.kotlin.toUri
import java.util.concurrent.TimeUnit

private const val CONNECT_TIMEOUT = 2L // Seconds
private const val READ_TIMEOUT = 10L // Seconds

/**
 * [IconLoader] implementation that will try to download the icon for resources that point to an http(s) URL.
 */
class HttpIconLoader(
    private val httpClient: Client
) : IconLoader {
    override fun load(context: Context, request: IconRequest, resource: IconRequest.Resource): IconLoader.Result {
        if (!resource.url.toUri().isHttpOrHttps) {
            return IconLoader.Result.NoResult
        }

        // Right now we always perform a download. We shouldn't retry to download from URLs that have failed just
        // recently: https://github.com/mozilla-mobile/android-components/issues/2591

        val downloadRequest = Request(
            url = resource.url,
            method = Request.Method.GET,
            cookiePolicy = Request.CookiePolicy.INCLUDE,
            connectTimeout = Pair(CONNECT_TIMEOUT, TimeUnit.SECONDS),
            readTimeout = Pair(READ_TIMEOUT, TimeUnit.SECONDS),
            redirect = Request.Redirect.FOLLOW,
            useCaches = true)

        val response = httpClient.fetch(downloadRequest)

        if (!response.isSuccess) {
            return IconLoader.Result.NoResult
        }

        return response.body.useStream {
            IconLoader.Result.BytesResult(it.readBytes(),
                Icon.Source.DOWNLOAD)
        }
    }
}
