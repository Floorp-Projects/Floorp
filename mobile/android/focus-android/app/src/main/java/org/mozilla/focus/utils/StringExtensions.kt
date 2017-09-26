/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils

import android.net.Uri

// Extension functions for the String class

internal object Constants {
    val ELLIPSIS_CHARACTER = '…'
}

/**
 * Beautify a URL by truncating it in a way that highlights important parts of the URL.
 *
 * Spec: https://github.com/mozilla-mobile/focus-android/issues/1231#issuecomment-326237077
 */
fun String.beautifyUrl(): String {
    if (!UrlUtils.isHttpOrHttps(this) || isNullOrEmpty()) {
        return this
    }

    val beautifulUrl = StringBuilder()

    val uri = Uri.parse(this)

    // We start with the host name:
    // * Pick the last two segments:
    //   en.m.wikipedia.org -> wikipedia.org
    // * If the last element has a length of 2 or less than take another segment:
    //   www.tomshardware.co.uk -> tomshardware.co.uk
    // * If there's no host then just remove the URL as is.

    val host = uri.host ?: return this
    val hostSegments = host.split(".")

    val usedHostSegments = mutableListOf<String>()
    for (segment in hostSegments.reversed()) {
        if (usedHostSegments.size < 2) {
            // We always pick the first two segments
            usedHostSegments.add(0, segment)
        } else if (usedHostSegments.first().length <= 2) {
            // The last segment has a length of 2 or less -> pick this segment too
            usedHostSegments.add(0, segment)
        } else {
            // We have everything we need. Bail out.
            break
        }
    }
    beautifulUrl.append(usedHostSegments.joinToString(separator = "."))

    // Now let's look at the path:
    // * We always take the first and the last segment and truncate everything in between:
    // /mozilla-mobile/focus-android/issues/1231 -> /mozilla-mobile/…/1231

    val pathSegments = uri.pathSegments

    for ((index, segment) in uri.pathSegments.withIndex()) {
        when (index) {
            0 -> {
                // Pick the first segment
                beautifulUrl.append("/")
                beautifulUrl.append(segment)
            }
            pathSegments.size - 1 -> {
                // Pick the last segment
                beautifulUrl.append("/")
                beautifulUrl.append(segment)
            }
            1 -> {
                // If this is the second element and there are more elements then insert the
                // ellipsis character here.
                beautifulUrl.append("/")
                beautifulUrl.append(Constants.ELLIPSIS_CHARACTER)
            }
        }
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
