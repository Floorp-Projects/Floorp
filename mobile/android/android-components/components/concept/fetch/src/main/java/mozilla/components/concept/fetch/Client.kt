/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.fetch

import android.util.Base64
import java.io.ByteArrayInputStream
import java.io.IOException
import java.net.URLDecoder
import java.nio.charset.Charset

/**
 * A generic [Client] for fetching resources via HTTP/s.
 *
 * Abstract base class / interface for clients implementing the `concept-fetch` component.
 *
 * The [Request]/[Response] API is inspired by the Web Fetch API:
 * https://developer.mozilla.org/en-US/docs/Web/API/Fetch_API
 */
abstract class Client {
    /**
     * Starts the process of fetching a resource from the network as described by the [Request] object. This call is
     * synchronous.
     *
     * A [Response] may keep references to open streams. Therefore it's important to always close the [Response] or
     * its [Response.Body].
     *
     * Use the `use()` extension method when performing multiple operations on the [Response] object:
     *
     * ```Kotlin
     * client.fetch(request).use { response ->
     *     // Use response. Resources will get released automatically at the end of the block.
     * }
     * ```
     *
     * Alternatively you can use multiple `use*()` methods on the [Response.Body] object.
     *
     * @param request The request to be executed by this [Client].
     * @return The [Response] returned by the server.
     * @throws IOException if the request could not be executed due to cancellation, a connectivity problem or a
     * timeout.
     */
    @Throws(IOException::class)
    abstract fun fetch(request: Request): Response

    /**
     * Generates a [Response] based on the provided [Request] for a data URI.
     *
     * @param request The [Request] for the data URI.
     * @return The generated [Response] including the decoded bytes as body.
     */
    @Suppress("ComplexMethod", "TooGenericExceptionCaught")
    protected fun fetchDataUri(request: Request): Response {
        if (!request.isDataUri()) {
            throw IOException("Not a data URI")
        }
        return try {
            val dataUri = request.url

            val (contentType, bytes) = if (dataUri.contains(DATA_URI_BASE64_EXT)) {
                dataUri.substringAfter(DATA_URI_SCHEME).substringBefore(DATA_URI_BASE64_EXT) to
                    Base64.decode(dataUri.substring(dataUri.lastIndexOf(',') + 1), Base64.DEFAULT)
            } else {
                val contentType = dataUri.substringAfter(DATA_URI_SCHEME).substringBefore(",")
                val charset = if (contentType.contains(DATA_URI_CHARSET)) {
                    Charset.forName(contentType.substringAfter(DATA_URI_CHARSET).substringBefore(","))
                } else {
                    Charsets.UTF_8
                }
                contentType to
                    URLDecoder.decode(dataUri.substring(dataUri.lastIndexOf(',') + 1), charset.name()).toByteArray()
            }

            val headers = MutableHeaders().apply {
                set(Headers.Names.CONTENT_LENGTH, bytes.size.toString())
                if (contentType.isNotEmpty()) {
                    set(Headers.Names.CONTENT_TYPE, contentType)
                }
            }

            Response(
                dataUri,
                Response.SUCCESS,
                headers,
                Response.Body(ByteArrayInputStream(bytes), contentType),
            )
        } catch (e: Exception) {
            throw IOException("Failed to decode data URI")
        }
    }

    companion object {
        const val DATA_URI_BASE64_EXT = ";base64"
        const val DATA_URI_SCHEME = "data:"
        const val DATA_URI_CHARSET = "charset="
    }
}
