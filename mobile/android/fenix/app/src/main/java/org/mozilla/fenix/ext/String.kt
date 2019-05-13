/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.fenix.ext

import java.net.MalformedURLException
import java.net.URL

/**
 * Replaces the keys with the values with the map provided.
 */
fun String.replace(pairs: Map<String, String>): String {
    var result = this
    pairs.forEach { (l, r) -> result = result.replace(l, r) }
    return result
}

/**
 * Try to parse and get host part if this [String] is valid URL.
 * Returns **null** otherwise.
 */
fun String?.getHostFromUrl(): String? = try {
    URL(this).host
} catch (e: MalformedURLException) {
    null
}

fun String?.urlToTrimmedHost(): String {
    return try {
        val url = URL(this)
        val firstIndex = url.host.indexOfFirst { it == '.' } + 1
        val lastIndex = url.host.indexOfLast { it == '.' }

        // Trim all but the title of the website from the hostname. 'www.mozilla.org' becomes 'mozilla'
        when {
            firstIndex - 1 == lastIndex -> url.host.substring(0, lastIndex)
            firstIndex < lastIndex -> url.host.substring(firstIndex, lastIndex)
            else -> url.host
        }
    } catch (e: MalformedURLException) {
        this.getHostFromUrl() ?: ""
    } catch (e: StringIndexOutOfBoundsException) {
        this.getHostFromUrl() ?: ""
    }
}
