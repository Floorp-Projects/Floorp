/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket

import android.net.Uri
import android.support.annotation.VisibleForTesting
import android.support.annotation.VisibleForTesting.PRIVATE
import mozilla.components.support.ktx.kotlin.toUri

private const val CDN_BASE = "https://getpocket.cdn.mozilla.net/v3/firefox"

/**
 * A collection of URLs available through the Pocket API.
 *
 * @param apiKey The Pocket API key: this is expected to be valid.
 */
internal class PocketURLs(
    private val apiKey: String
) {

    val globalVideoRecs: Uri = "$CDN_BASE/global-video-recs".toUri().appendPocketQueryParams()

    private fun Uri.appendPocketQueryParams(): Uri = buildUpon()
        .appendQueryParameter(PARAM_API_KEY, apiKey)
        .appendQueryParameter(PARAM_VERSION, VALUE_VERSION)
        .appendQueryParameter(PARAM_AUTHORS, VALUE_AUTHORS)
        .build()

    companion object {
        @VisibleForTesting(otherwise = PRIVATE) internal const val PARAM_API_KEY = "consumer_key"
        @VisibleForTesting(otherwise = PRIVATE) internal const val PARAM_VERSION = "version"
        @VisibleForTesting(otherwise = PRIVATE) internal const val PARAM_AUTHORS = "authors"

        @VisibleForTesting(otherwise = PRIVATE) internal const val VALUE_VERSION = "2"
        @VisibleForTesting(otherwise = PRIVATE) internal const val VALUE_AUTHORS = "1"
    }
}
