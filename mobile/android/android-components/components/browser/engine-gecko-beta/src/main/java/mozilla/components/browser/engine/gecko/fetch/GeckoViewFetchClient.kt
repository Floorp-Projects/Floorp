/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.fetch

import android.content.Context
import androidx.annotation.VisibleForTesting
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Headers
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response

import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoWebExecutor
import org.mozilla.geckoview.WebRequest
import org.mozilla.geckoview.WebRequest.CACHE_MODE_DEFAULT
import org.mozilla.geckoview.WebRequest.CACHE_MODE_RELOAD
import org.mozilla.geckoview.WebRequestError
import org.mozilla.geckoview.WebResponse
import java.io.IOException
import java.net.SocketTimeoutException
import java.nio.ByteBuffer
import java.util.concurrent.TimeUnit
import java.util.concurrent.TimeoutException

/**
 * GeckoView ([GeckoWebExecutor]) based implementation of [Client].
 */
class GeckoViewFetchClient(
    context: Context,
    runtime: GeckoRuntime = GeckoRuntime.getDefault(context),
    private val maxReadTimeOut: Pair<Long, TimeUnit> = Pair(MAX_READ_TIMEOUT_MINUTES, TimeUnit.MINUTES)
) : Client() {

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var executor: GeckoWebExecutor = GeckoWebExecutor(runtime)

    @Throws(IOException::class)
    override fun fetch(request: Request): Response {
        val webRequest = request.toWebRequest(defaultHeaders)

        val readTimeOut = request.readTimeout ?: maxReadTimeOut
        val readTimeOutMillis = readTimeOut.let { (timeout, unit) ->
            unit.toMillis(timeout)
        }

        return try {
            var fetchFlags = 0
            if (request.cookiePolicy == Request.CookiePolicy.OMIT) {
                fetchFlags += GeckoWebExecutor.FETCH_FLAGS_ANONYMOUS
            }
            if (request.redirect == Request.Redirect.MANUAL) {
                fetchFlags += GeckoWebExecutor.FETCH_FLAGS_NO_REDIRECTS
            }
            val webResponse = executor.fetch(webRequest, fetchFlags).poll(readTimeOutMillis)
            webResponse?.toResponse() ?: throw IOException("Fetch failed with null response")
        } catch (e: TimeoutException) {
            throw SocketTimeoutException()
        } catch (e: WebRequestError) {
            throw IOException(e)
        }
    }

    companion object {
        const val MAX_READ_TIMEOUT_MINUTES = 5L
    }
}

private fun Request.toWebRequest(defaultHeaders: Headers): WebRequest = WebRequest.Builder(url)
    .method(method.name)
    .addHeadersFrom(this, defaultHeaders)
    .addBodyFrom(this)
    .cacheMode(if (useCaches) CACHE_MODE_DEFAULT else CACHE_MODE_RELOAD)
    .build()

private fun WebRequest.Builder.addHeadersFrom(request: Request, defaultHeaders: Headers): WebRequest.Builder {
    defaultHeaders.filter { header ->
        request.headers?.contains(header.name) != true
    }.forEach { header ->
        addHeader(header.name, header.value)
    }

    request.headers?.forEach { header ->
        addHeader(header.name, header.value)
    }

    return this
}

private fun WebRequest.Builder.addBodyFrom(request: Request): WebRequest.Builder {
    request.body?.let { body ->
        body.useStream { inStream ->
            val bytes = inStream.readBytes()
            val buffer = ByteBuffer.allocateDirect(bytes.size)
            buffer.put(bytes)
            this.body(buffer)
        }
    }

    return this
}

private fun WebResponse.toResponse(): Response {
    val headers = translateHeaders(this)
    return Response(
        uri,
        statusCode,
        headers,
            body?.let {
                Response.Body(it, headers["Content-Type"])
            } ?: Response.Body.empty()
    )
}

private fun translateHeaders(webResponse: WebResponse): Headers {
    val headers = MutableHeaders()
    webResponse.headers.forEach { (k, v) ->
        v.split(",").forEach { headers.append(k, it.trim()) }
    }

    return headers
}
