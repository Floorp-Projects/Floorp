/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.telemetry

import java.util.Locale

/**
 * Key information about a Search Engine Result Page (SERP).
 *
 * @property provider The name of the search provider.
 * @property type The search access point type (SAP). This is either "organic", "sap" or
 * "sap-follow-on".
 * @property code The search URL's `code` query parameter.
 * @property channel The search URL's `channel` query parameter.
 */
internal data class TrackKeyInfo(
    var provider: String,
    var type: String,
    var code: String?,
    var channel: String? = null,
) {
    /**
     * Returns the track key information into the following string format:
     * `<provider>.in-content.[sap|sap-follow-on|organic].[code|none](.[channel])?`.
     */
    fun createTrackKey(): String {
        return "${provider.lowercase(Locale.ROOT)}.in-content" +
            ".${type.lowercase(Locale.ROOT)}" +
            ".${code?.lowercase(Locale.ROOT) ?: "none"}" +
            if (!channel?.lowercase(Locale.ROOT).isNullOrBlank()) {
                ".${channel?.lowercase(Locale.ROOT)}"
            } else {
                ""
            }
    }
}
