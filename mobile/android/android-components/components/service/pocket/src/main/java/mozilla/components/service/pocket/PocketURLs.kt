/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket

import android.net.Uri
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PRIVATE
import androidx.core.net.toUri

private const val CDN_BASE = "https://getpocket.cdn.mozilla.net/v3/firefox"

/**
 * A collection of URLs available through the Pocket API.
 *
 * @param apiKey The Pocket API key: this is expected to be valid.
 */
internal class PocketURLs(
    private val apiKey: String
) {

    val globalVideoRecs: Uri = "$CDN_BASE/global-video-recs".toUri().buildUpon()
        .appendApiKey()
        .appendQueryParameter(PARAM_VERSION, VALUE_VIDEO_RECS_VERSION)
        .appendQueryParameter(PARAM_AUTHORS, VALUE_VIDEO_RECS_AUTHORS)
        .build()

    private fun Uri.Builder.appendApiKey() = appendQueryParameter(PARAM_API_KEY, apiKey)

    companion object {
        @VisibleForTesting(otherwise = PRIVATE) internal const val PARAM_API_KEY = "consumer_key"
        @VisibleForTesting(otherwise = PRIVATE) internal const val PARAM_VERSION = "version"
        @VisibleForTesting(otherwise = PRIVATE) internal const val PARAM_AUTHORS = "authors"

        @VisibleForTesting(otherwise = PRIVATE) internal const val VALUE_VIDEO_RECS_VERSION = "2"
        @VisibleForTesting(otherwise = PRIVATE) internal const val VALUE_VIDEO_RECS_AUTHORS = "1"
    }
}
