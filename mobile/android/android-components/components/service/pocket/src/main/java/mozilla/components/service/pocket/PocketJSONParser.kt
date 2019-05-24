/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket

import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PRIVATE
import mozilla.components.service.pocket.data.PocketGlobalVideoRecommendation
import mozilla.components.support.ktx.android.org.json.mapNotNull
import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject

/**
 * Holds functions that parse the JSON returned by the Pocket API and converts them to more usable Kotlin types.
 */
class PocketJSONParser internal constructor() {

    /**
     * @return The videos, removing entries that are invalid, or null on error; the list will never be empty.
     */
    fun jsonToGlobalVideoRecommendations(jsonStr: String): List<PocketGlobalVideoRecommendation>? = try {
        val rawJSON = JSONObject(jsonStr)
        val videosJSON = rawJSON.getJSONArray(KEY_VIDEO_RECOMMENDATIONS_INNER)
        val videos = videosJSON.mapNotNull(JSONArray::getJSONObject) { jsonToGlobalVideoRecommendation(it) }

        // We return null, rather than the empty list, because devs might forget to check an empty list.
        if (videos.isNotEmpty()) videos else null
    } catch (e: JSONException) {
        logger.warn("invalid JSON from Pocket server", e)
        null
    }

    private fun jsonToGlobalVideoRecommendation(jsonObj: JSONObject): PocketGlobalVideoRecommendation? = try {
        /** @return a list of authors, removing entries which are invalid JSON */
        fun getAuthors(authorsObj: JSONObject): List<PocketGlobalVideoRecommendation.Author> {
            return authorsObj.keys().asSequence().toList().mapNotNull { key ->
                try {
                    val authorJson = authorsObj.getJSONObject(key)
                    PocketGlobalVideoRecommendation.Author(
                        id = authorJson.getString("author_id"),
                        name = authorJson.getString("name"),
                        url = authorJson.getString("url")
                    )
                } catch (e: JSONException) {
                    logger.warn("Invalid author object in pocket JSON", e)
                    null
                }
            }
        }

        PocketGlobalVideoRecommendation(
            id = jsonObj.getLong("id"),
            url = jsonObj.getString("url"),
            tvURL = jsonObj.getString("tv_url"),
            title = jsonObj.getString("title"),
            excerpt = jsonObj.getString("excerpt"),
            domain = jsonObj.getString("domain"),
            imageSrc = jsonObj.getString("image_src"),
            publishedTimestamp = jsonObj.getString("published_timestamp"),
            sortId = jsonObj.getInt("sort_id"),
            popularitySortId = jsonObj.getInt("popularity_sort_id"),
            authors = getAuthors(jsonObj.getJSONObject("authors"))
        )
    } catch (e: JSONException) {
        null
    }

    companion object {
        @VisibleForTesting(otherwise = PRIVATE) const val KEY_VIDEO_RECOMMENDATIONS_INNER = "recommendations"

        /**
         * Returns a new instance of [PocketJSONParser].
         */
        fun newInstance(): PocketJSONParser {
            return PocketJSONParser()
        }
    }
}
