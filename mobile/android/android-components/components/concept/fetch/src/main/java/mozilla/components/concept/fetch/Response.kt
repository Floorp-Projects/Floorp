/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.fetch

import mozilla.components.concept.fetch.Response.Body
import mozilla.components.concept.fetch.Response.Companion.CLIENT_ERROR_STATUS_RANGE
import mozilla.components.concept.fetch.Response.Companion.SUCCESS_STATUS_RANGE
import java.io.BufferedReader
import java.io.Closeable
import java.io.IOException
import java.io.InputStream
import java.nio.charset.Charset

/**
 * The [Response] data class represents a response to a [Request] send by a [Client].
 *
 * You can create a [Response] object using the constructor, but you are more likely to encounter a [Response] object
 * being returned as the result of calling [Client.fetch].
 *
 * A [Response] may hold references to other resources (e.g. streams). Therefore it's important to always close the
 * [Response] object or its [Body]. This can be done by either consuming the content of the [Body] with one of the
 * available methods or by using Kotlin's extension methods for using [Closeable] implementations (like `use()`):
 *
 * ```Kotlin
 * val response = ...
 * response.use {
 *    // Use response. Resources will get released automatically at the end of the block.
 * }
 * ```
 */
data class Response(
    val url: String,
    val status: Int,
    val headers: Headers,
    val body: Body,
) : Closeable {
    /**
     * Closes this [Response] and its [Body] and releases any system resources associated with it.
     */
    override fun close() {
        body.close()
    }

    /**
     * A [Body] returned along with the [Request].
     *
     * **The response body can be consumed only once.**.
     *
     * @param stream the input stream from which the response body can be read.
     * @param contentType optional content-type as provided in the response
     * header. If specified, an attempt will be made to look up the charset
     * which will be used for decoding the body. If not specified, or if the
     * charset can't be found, UTF-8 will be used for decoding.
     */
    open class Body(
        private val stream: InputStream,
        contentType: String? = null,
    ) : Closeable, AutoCloseable {

        @Suppress("TooGenericExceptionCaught")
        private val charset = contentType?.let {
            val charset = it.substringAfter("charset=")
            try {
                Charset.forName(charset)
            } catch (e: Exception) {
                Charsets.UTF_8
            }
        } ?: Charsets.UTF_8

        /**
         * Creates a usable stream from this body.
         *
         * Executes the given [block] function with the stream as parameter and then closes it down correctly
         * whether an exception is thrown or not.
         */
        fun <R> useStream(block: (InputStream) -> R): R = use {
            block(stream)
        }

        /**
         * Creates a buffered reader from this body.
         *
         * Executes the given [block] function with the buffered reader as parameter and then closes it down correctly
         * whether an exception is thrown or not.
         *
         * @param charset the optional charset to use when decoding the body. If not specified,
         * the charset provided in the response content-type header will be used. If the header
         * is missing or the charset is not supported, UTF-8 will be used.
         * @param block a function to consume the buffered reader.
         *
         */
        fun <R> useBufferedReader(charset: Charset? = null, block: (BufferedReader) -> R): R = use {
            block(stream.bufferedReader(charset ?: this.charset))
        }

        /**
         * Reads this body completely as a String.
         *
         * Takes care of closing the body down correctly whether an exception is thrown or not.
         *
         * @param charset the optional charset to use when decoding the body. If not specified,
         * the charset provided in the response content-type header will be used. If the header
         * is missing or the charset not supported, UTF-8 will be used.
         */
        fun string(charset: Charset? = null): String = use {
            // We don't use a BufferedReader because it'd unnecessarily allocate more memory: if the
            // BufferedReader is reading into a buffer whose length >= the BufferedReader's buffer
            // length, then the BufferedReader reads directly into the other buffer as an optimization
            // and the BufferedReader's buffer is unused (i.e. you get no benefit from the BufferedReader
            // and you can just use a Reader). In this case, both the BufferedReader and readText
            // would allocate a buffer of DEFAULT_BUFFER_SIZE so we removed the unnecessary
            // BufferedReader and cut memory consumption in half. See
            // https://github.com/mcomella/android-components/commit/db8488599f9f652b4d5775f70eeb4ab91462cbe6
            // for code verifying this behavior.
            //
            // The allocation can be further optimized by setting the buffer size to Content-Length
            // header. See https://github.com/mozilla-mobile/android-components/issues/11015
            stream.reader(charset ?: this.charset).readText()
        }

        /**
         * Closes this [Body] and releases any system resources associated with it.
         */
        override fun close() {
            try {
                stream.close()
            } catch (e: IOException) {
                // Ignore
            }
        }

        companion object {
            /**
             * Creates an empty response body.
             */
            fun empty() = Body("".byteInputStream())
        }
    }

    companion object {
        val SUCCESS_STATUS_RANGE = 200..299
        val CLIENT_ERROR_STATUS_RANGE = 400..499
        const val SUCCESS = 200
    }
}

/**
 * Returns true if the response was successful (status in the range 200-299) or false otherwise.
 */
val Response.isSuccess: Boolean
    get() = status in SUCCESS_STATUS_RANGE

/**
 * Returns true if the response was a client error (status in the range 400-499) or false otherwise.
 */
val Response.isClientError: Boolean
    get() = status in CLIENT_ERROR_STATUS_RANGE
