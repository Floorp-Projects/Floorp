/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.accounts.push.ext

import android.net.Uri

/**
 * Removes all expect the [lastSegmentToTake] of the [Uri.getLastPathSegment] result.
 *
 * Example:
 * ```
 * val url = https://mozilla.org/firefox
 * val redactedUrl = url.redactPartialUri(3, "redacted...")
 *
 * assertEquals("https://mozilla.org/redacted...fox", redactedUrl)
 * ```
 */
fun String.redactPartialUri(lastSegmentToTake: Int = 20, shortForm: String = "redacted..."): String {
    val uri = Uri.parse(this)
    val end = shortForm + uri.lastPathSegment?.takeLast(lastSegmentToTake)
    val path = uri
        .pathSegments
        .take(uri.pathSegments.size - 1)
        .joinToString("/")

    return uri
        .buildUpon()
        .path(path)
        .appendPath(end)
        .build()
        .toString()
}
