/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.digitalassetlinks.api

import org.json.JSONObject

/**
 * @property linked True if the assets specified in the request are linked by the relation specified in the request.
 * @property maxAge From serving time, how much longer the response should be considered valid barring further updates.
 * Formatted as a duration in seconds with up to nine fractional digits, terminated by 's'. Example: "3.5s".
 * @property debug Human-readable message containing information about the response.
 */
data class CheckAssetLinksResponse(
    val linked: Boolean,
    val maxAge: String,
    val debug: String,
)

internal fun parseCheckAssetLinksJson(json: JSONObject) = CheckAssetLinksResponse(
    linked = json.getBoolean("linked"),
    maxAge = json.getString("maxAge"),
    debug = json.optString("debugString"),
)
