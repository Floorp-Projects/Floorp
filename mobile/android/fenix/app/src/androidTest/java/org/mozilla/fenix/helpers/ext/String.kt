/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers.ext

import android.net.Uri
import java.net.URI
import java.net.URISyntaxException

// Extension functions for the String class

/**
 * If this string starts with the one or more of the given [prefixes] (in order and ignoring case),
 * returns a copy of this string with the prefixes removed. Otherwise, returns this string.
 */
fun String.removePrefixesIgnoreCase(vararg prefixes: String): String {
    var value = this
    var lower = this.lowercase()

    prefixes.forEach {
        if (lower.startsWith(it.lowercase())) {
            value = value.substring(it.length)
            lower = lower.substring(it.length)
        }
    }

    return value
}

fun String?.toUri(): Uri? = if (this == null) {
    null
} else {
    Uri.parse(this)
}

fun String?.toJavaURI(): URI? = if (this == null) {
    null
} else {
    try {
        URI(this)
    } catch (e: URISyntaxException) {
        null
    }
}
