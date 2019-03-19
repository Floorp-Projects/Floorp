/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.fetch

import java.lang.IllegalArgumentException

/**
 * A collection of HTTP [Headers] (immutable) of a [Request] or [Response].
 */
interface Headers : Iterable<Header> {
    /**
     * Returns the number of headers (key / value combinations).
     */
    val size: Int

    /**
     * Gets the [Header] at the specified [index].
     */
    operator fun get(index: Int): Header

    /**
     * Returns the last values corresponding to the specified header field name. Or null if the header does not exist.
     */
    operator fun get(name: String): String?

    /**
     * Returns the list of values corresponding to the specified header field name.
     */
    fun getAll(name: String): List<String>

    /**
     * Sets the [Header] at the specified [index].
     */
    operator fun set(index: Int, header: Header)

    /**
     * Returns true if a [Header] with the given [name] exists.
     */
    operator fun contains(name: String): Boolean

    /**
     * A collection of common HTTP header names.
     *
     * A list of common HTTP request headers can be found at
     *   https://en.wikipedia.org/wiki/List_of_HTTP_header_fields#Standard_request_fields
     *
     * A list of common HTTP response headers can be found at
     *   https://en.wikipedia.org/wiki/List_of_HTTP_header_fields#Standard_response_fields
     */
    object Common {
        const val CONTENT_TYPE = "Content-Type"
        const val USER_AGENT = "User-Agent"

        /*
         * A collection of common HTTP header values.
         */
        object Value {
            const val CONTENT_TYPE_FORM_URLENCODED = "application/x-www-form-urlencoded"
        }
    }
}

/**
 * Represents a [Header] containing of a [name] and [value].
 */
data class Header(
    val name: String,
    val value: String
) {
    init {
        if (name.isEmpty()) {
            throw IllegalArgumentException("Header name cannot be empty")
        }
    }
}

/**
 * A collection of HTTP [Headers] (mutable) of a [Request] or [Response].
 */
class MutableHeaders(
    vararg pairs: Pair<String, String>
) : Headers, MutableIterable<Header> {
    private val headers: MutableList<Header> = pairs.map {
            (name, value) -> Header(name, value)
    }.toMutableList()

    /**
     * Gets the [Header] at the specified [index].
     */
    override fun get(index: Int): Header = headers[index]

    /**
     * Returns the last value corresponding to the specified header field name. Or null if the header does not exist.
     */
    override fun get(name: String) = headers.lastOrNull { it.name.toLowerCase() == name.toLowerCase() }?.value

    /**
     * Returns the list of values corresponding to the specified header field name.
     */
    override fun getAll(name: String): List<String> = headers
        .filter { header -> header.name == name }
        .map { header -> header.value }

    /**
     * Sets the [Header] at the specified [index].
     */
    override fun set(index: Int, header: Header) {
        headers[index] = header
    }

    /**
     * Returns an iterator over the headers that supports removing elements during iteration.
     */
    override fun iterator(): MutableIterator<Header> = headers.iterator()

    /**
     * Returns true if a [Header] with the given [name] exists.
     */
    override operator fun contains(name: String): Boolean = headers.firstOrNull { it.name == name } != null

    /**
     * Returns the number of headers (key / value combinations).
     */
    override val size: Int
        get() = headers.size

    /**
     * Append a header without removing the headers already present.
     */
    fun append(name: String, value: String): MutableHeaders {
        headers.add(Header(name, value))
        return this
    }

    /**
     * Set the only occurrence of the header; potentially overriding an already existing header.
     */
    fun set(name: String, value: String): MutableHeaders {
        headers.forEachIndexed { index, current ->
            if (current.name == name) {
                headers[index] = Header(name, value)
                return this
            }
        }

        return append(name, value)
    }
}
