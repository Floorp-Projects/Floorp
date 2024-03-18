/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.domains

/**
 * Class intended for internal use which encapsulates meta data about a domain.
 */
data class Domain(val protocol: String, val hasWww: Boolean, val host: String) {
    internal val url: String
        get() = "$protocol${if (hasWww) "www." else "" }$host"

    companion object {
        private const val PROTOCOL_INDEX = 1
        private const val WWW_INDEX = 2
        private const val HOST_INDEX = 3

        private const val DEFAULT_PROTOCOL = "http://"

        private val urlMatcher = Regex("""(https?://)?(www.)?(.+)?""")

        fun create(url: String): Domain {
            val result = urlMatcher.find(url)

            return result?.let {
                val protocol = it.groups[PROTOCOL_INDEX]?.value ?: DEFAULT_PROTOCOL
                val hasWww = it.groups[WWW_INDEX]?.value == "www."
                val host = it.groups[HOST_INDEX]?.value ?: throw IllegalStateException()

                return Domain(protocol, hasWww, host)
            } ?: throw IllegalStateException()
        }
    }
}

internal fun Iterable<String>.into(): List<Domain> {
    return this.map { Domain.create(it) }
}
