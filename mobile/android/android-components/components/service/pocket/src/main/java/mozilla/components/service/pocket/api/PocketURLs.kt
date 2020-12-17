/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.api

import android.net.Uri
import androidx.annotation.VisibleForTesting
import androidx.core.net.toUri

private const val CDN_BASE = "https://getpocket.cdn.mozilla.net/v3/firefox"

/**
 * A collection of URLs available through the Pocket API.
 *
 * @param apiKey The Pocket API key: this is expected to be valid.
 */
internal class PocketURLs(
    @VisibleForTesting internal val apiKey: String
) {

    fun getLocaleStoriesRecommendations(count: Int, locale: String): Uri {
        return "$CDN_BASE/global-recs".toUri().buildUpon()
            .appendQueryParameter(PARAM_API_KEY, apiKey)
            .appendQueryParameter(PARAM_COUNT, count.toString())
            .appendQueryParameter(PARAM_LOCALE, locale)
            .build()
    }

    @VisibleForTesting
    companion object {
        @VisibleForTesting const val PARAM_API_KEY = "consumer_key"
        @VisibleForTesting const val PARAM_COUNT = "count"
        @VisibleForTesting const val PARAM_LOCALE = "locale"
    }
}
