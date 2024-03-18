/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.fetch.httpurlconnection

import mozilla.components.concept.fetch.BuildConfig
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Headers
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.concept.fetch.isDataUri
import mozilla.components.lib.fetch.httpurlconnection.HttpURLConnectionClient.Companion.getOrCreateCookieManager
import java.io.FileNotFoundException
import java.io.IOException
import java.io.InputStream
import java.net.CookieHandler
import java.net.CookieManager
import java.net.HttpURLConnection
import java.net.URL
import java.util.zip.GZIPInputStream

/**
 * [HttpURLConnection] implementation of [Client].
 */
class HttpURLConnectionClient : Client() {
    private val defaultHeaders: Headers = MutableHeaders(
        "User-Agent" to "MozacFetch/${BuildConfig.LIBRARY_VERSION}",
        "Accept-Encoding" to "gzip",
    )

    @Throws(IOException::class)
    override fun fetch(request: Request): Response {
        if (request.private) {
            throw IllegalArgumentException("Client doesn't support private request")
        }
        if (request.isDataUri()) {
            return fetchDataUri(request)
        }

        val connection = (URL(request.url).openConnection() as HttpURLConnection)

        connection.setupWith(request)
        connection.addHeadersFrom(request, defaultHeaders)
        connection.addBodyFrom(request)

        return connection.toResponse()
    }

    companion object {
        fun getOrCreateCookieManager(): CookieManager {
            if (CookieHandler.getDefault() == null) {
                CookieHandler.setDefault(CookieManager())
            }
            return CookieHandler.getDefault() as CookieManager
        }
    }
}

private fun HttpURLConnection.addBodyFrom(request: Request) {
    if (request.body == null) {
        return
    }

    request.body?.let { body ->
        doOutput = true

        body.useStream { inStream ->
            outputStream.use { outStream ->
                inStream
                    .buffered()
                    .copyTo(outStream)
                outStream.flush()
            }
        }
    }
}

internal fun HttpURLConnection.setupWith(request: Request) {
    requestMethod = request.method.name
    instanceFollowRedirects = request.redirect == Request.Redirect.FOLLOW

    request.connectTimeout?.let { (timeout, unit) ->
        connectTimeout = unit.toMillis(timeout).toInt()
    }

    request.readTimeout?.let { (timeout, unit) ->
        readTimeout = unit.toMillis(timeout).toInt()
    }

    useCaches = request.useCaches

    // HttpURLConnection can't be configured to omit cookies. As
    // a workaround, we delete all cookies we have stored for
    // the request URI.
    val cookieManager = getOrCreateCookieManager()
    if (request.cookiePolicy == Request.CookiePolicy.OMIT) {
        val uri = URL(request.url).toURI()
        for (cookie in cookieManager.cookieStore.get(uri)) {
            cookieManager.cookieStore.remove(uri, cookie)
        }
    }
}

private fun HttpURLConnection.addHeadersFrom(request: Request, defaultHeaders: Headers) {
    defaultHeaders.filter { header ->
        request.headers?.contains(header.name) != true
    }.forEach { header ->
        setRequestProperty(header.name, header.value)
    }

    request.headers?.forEach { header ->
        addRequestProperty(header.name, header.value)
    }
}

private fun HttpURLConnection.toResponse(): Response {
    val headers = translateHeaders(this)
    return Response(
        url.toString(),
        responseCode,
        headers,
        createBody(this, headers["Content-Type"]),
    )
}

private fun translateHeaders(connection: HttpURLConnection): Headers {
    val headers = MutableHeaders()

    var index = 0

    while (connection.getHeaderField(index) != null) {
        val name = connection.getHeaderFieldKey(index)
        if (name == null) {
            index++
            continue
        }

        val value = connection.getHeaderField(index)

        headers.append(name, value)

        index++
    }

    return headers
}

private fun createBody(connection: HttpURLConnection, contentType: String?): Response.Body {
    val gzipped = connection.contentEncoding == "gzip"

    withFileNotFoundExceptionIgnored {
        return HttpUrlConnectionBody(
            connection,
            connection.inputStream,
            gzipped,
            contentType,
        )
    }

    withFileNotFoundExceptionIgnored {
        return HttpUrlConnectionBody(
            connection,
            connection.errorStream,
            gzipped,
            contentType,
        )
    }

    return EmptyBody()
}

private class EmptyBody : Response.Body("".byteInputStream())

private class HttpUrlConnectionBody(
    private val connection: HttpURLConnection,
    stream: InputStream,
    gzipped: Boolean,
    contentType: String?,
) : Response.Body(if (gzipped) GZIPInputStream(stream) else stream, contentType) {
    override fun close() {
        super.close()

        connection.disconnect()
    }
}

private inline fun withFileNotFoundExceptionIgnored(block: () -> Unit) {
    try {
        block()
    } catch (e: FileNotFoundException) {
        // Ignore
    }
}
