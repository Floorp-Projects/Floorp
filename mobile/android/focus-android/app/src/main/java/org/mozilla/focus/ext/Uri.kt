/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.ext

import android.net.Uri
import mozilla.components.ktx.kotlin.ELLIPSIS

// Extension functions for the android.net.Uri class

/**
 * Return the truncated host of this Uri. The truncated host will only contain up to 2-3 segments of
 * the original host. The original host will be returned if it's null or and empty String.
 *
 * Examples:
 *   mail.google.com -> google.com
 *   www.tomshardware.co.uk -> tomshardware.co.uk
 *
 * Spec: https://github.com/mozilla-mobile/focus-android/issues/1231#issuecomment-326237077
 */
fun Uri.truncatedHost(): String? {
    if (host.isNullOrEmpty()) {
        return host
    }

    // We start with the host name:
    // * Pick the last two segments:
    //   en.m.wikipedia.org -> wikipedia.org
    // * If the last element has a length of 2 or less than take another segment:
    //   www.tomshardware.co.uk -> tomshardware.co.uk
    // * If there's no host then just remove the URL as is.

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

    return usedHostSegments.joinToString(separator = ".")
}

/**
 * Return the truncated path only containing the first and last segment of the full path. If the
 * Uri does not have a path an empty String will be returned.
 *
 * Example:
 * /foo/bar/test/index.html -> /foo/…/index.html
 */
fun Uri.truncatedPath(): String {
    val segments = pathSegments

    return when (segments.size) {
        0 -> ""
        1 -> "/" + segments[0]
        2 -> "/" + segments.joinToString(separator = "/" )
        else -> "/" + listOf(segments.first(), Char.ELLIPSIS, segments.last()).joinToString(separator = "/" )
    }
}
