/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.fetch

import android.content.Context
import android.support.annotation.VisibleForTesting
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Headers
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response

import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.GeckoWebExecutor
import org.mozilla.geckoview.WebRequest
import org.mozilla.geckoview.WebResponse
import java.io.IOException
import java.io.InputStream
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
        val webRequest = with(request) {
            WebRequest.Builder(url)
                .method(method.name)
                .addHeadersFrom(request, defaultHeaders)
                .addBodyFrom(request)
                .build()
        }

        val readTimeOut = request.readTimeout ?: maxReadTimeOut
        val readTimeOutMillis = readTimeOut.let { (timeout, unit) ->
            unit.toMillis(timeout)
        }

        try {
            val webResponse = executor.fetch(webRequest).poll(readTimeOutMillis)
            return webResponse?.toResponse() ?: throw IOException("Fetch failed with null response")
        } catch (e: TimeoutException) {
            throw SocketTimeoutException()
        }
    }

    companion object {
        const val MAX_READ_TIMEOUT_MINUTES = 5L
    }
}

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
    return Response(
        uri,
        statusCode,
        translateHeaders(this),
            body?.let { Response.Body(ByteBufferInputStream(it)) } ?: Response.Body.empty()
    )
}

private fun translateHeaders(webResponse: WebResponse): Headers {
    val headers = MutableHeaders()
    webResponse.headers.forEach { (k, v) ->
        v.split(",").forEach { headers.append(k, it.trim()) }
    }

    return headers
}

class ByteBufferInputStream(private var buf: ByteBuffer) : InputStream() {
    @Throws(IOException::class)
    @Suppress("MagicNumber")
    override fun read(): Int {
        return if (!buf.hasRemaining()) -1 else (buf.get().toInt() and 0xFF)
    }

    @Throws(IOException::class)
    override fun read(bytes: ByteArray, off: Int, len: Int): Int {
        var len = len
        if (!buf.hasRemaining()) {
            return -1
        }

        len = Math.min(len, buf.remaining())
        buf.get(bytes, off, len)
        return len
    }
}
