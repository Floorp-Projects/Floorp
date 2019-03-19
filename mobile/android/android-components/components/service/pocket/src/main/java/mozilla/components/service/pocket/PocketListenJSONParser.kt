/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket

import mozilla.components.service.pocket.data.PocketListenArticleMetadata
import mozilla.components.service.pocket.data.PocketListenArticleMetadata.Status
import org.json.JSONArray
import org.json.JSONException

/**
 * Holds functions that parse the JSON returned by the Pocket Listen API and converts them to more usable Kotlin types.
 */
internal class PocketListenJSONParser {

    /**
     * @return the article's listen metadata or null on error.
     */
    fun jsonToListenArticleMetadata(jsonStr: String): PocketListenArticleMetadata? = try {
        val rawJSON = JSONArray(jsonStr)
        val innerObject = rawJSON.getJSONObject(0)
        PocketListenArticleMetadata(
            format = innerObject.getString("format"),
            audioUrl = innerObject.getString("url"),
            status = Status.fromString(innerObject.getString("status")),
            voice = innerObject.getString("voice"),
            durationSeconds = innerObject.getLong("duration"),
            size = innerObject.getString("size")
        )
    } catch (e: JSONException) {
        logger.warn("invalid JSON from Pocket server", e)
        null
    }
}
