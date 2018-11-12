/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.ext

import android.net.Uri
import org.mozilla.focus.utils.UrlUtils

// Extension functions for the String class

/**
 * Beautify a URL by truncating it in a way that highlights important parts of the URL.
 *
 * Spec: https://github.com/mozilla-mobile/focus-android/issues/1231#issuecomment-326237077
 */
fun String.beautifyUrl(): String {
    if (isNullOrEmpty() || !UrlUtils.isHttpOrHttps(this)) {
        return this
    }

    val beautifulUrl = StringBuilder()

    val uri = Uri.parse(this)

    // Use only the truncated host name

    val truncatedHost = uri.truncatedHost()
    if (truncatedHost.isNullOrEmpty()) {
        return this
    }

    beautifulUrl.append(truncatedHost)

    // Append the truncated path

    val truncatedPath = uri.truncatedPath()
    if (!truncatedPath.isNullOrEmpty()) {
        beautifulUrl.append(truncatedPath)
    }

    // And then append (only) the first query parameter

    val query = uri.query
    if (!query.isNullOrEmpty()) {
        beautifulUrl.append("?")
        beautifulUrl.append(query.split("&").first())
    }

    // We always append a fragment if there's one

    val fragment = uri.fragment
    if (!fragment.isNullOrEmpty()) {
        beautifulUrl.append("#")
        beautifulUrl.append(fragment)
    }

    return beautifulUrl.toString()
}

/**
 * If this string starts with the one or more of the given [prefixes] (in order and ignoring case),
 * returns a copy of this string with the prefixes removed. Otherwise, returns this string.
 */
fun String.removePrefixesIgnoreCase(vararg prefixes: String): String {
    var value = this
    var lower = this.toLowerCase()

    prefixes.forEach {
        if (lower.startsWith(it.toLowerCase())) {
            value = value.substring(it.length)
            lower = lower.substring(it.length)
        }
    }

    return value
}
