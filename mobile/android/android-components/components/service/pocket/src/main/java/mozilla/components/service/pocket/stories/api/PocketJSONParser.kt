/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.stories.api

import androidx.annotation.VisibleForTesting
import mozilla.components.service.pocket.logger
import mozilla.components.support.ktx.android.org.json.mapNotNull
import mozilla.components.support.ktx.android.org.json.tryGetInt
import mozilla.components.support.ktx.android.org.json.tryGetString
import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject

private const val JSON_STORY_TITLE_KEY = "title"
private const val JSON_STORY_URL_KEY = "url"
private const val JSON_STORY_IMAGE_URL_KEY = "imageUrl"
private const val JSON_STORY_PUBLISHER_KEY = "publisher"
private const val JSON_STORY_CATEGORY_KEY = "category"
private const val JSON_STORY_TIME_TO_READ_KEY = "timeToRead"

/**
 * Holds functions that parse the JSON returned by the Pocket API and converts them to more usable Kotlin types.
 */
internal class PocketJSONParser {
    /**
     * @return The stories, removing entries that are invalid, or null on error; the list will never be empty.
     */
    fun jsonToPocketApiStories(json: String): List<PocketApiStory>? = try {
        val rawJSON = JSONObject(json)
        val storiesJSON = rawJSON.getJSONArray(KEY_ARRAY_ITEMS)
        val stories = storiesJSON.mapNotNull(JSONArray::getJSONObject) { jsonToPocketApiStory(it) }

        // We return null, rather than the empty list, because devs might forget to check an empty list.
        if (stories.isNotEmpty()) stories else null
    } catch (e: JSONException) {
        logger.warn("invalid JSON from the Pocket endpoint", e)
        null
    }

    private fun jsonToPocketApiStory(json: JSONObject): PocketApiStory? = try {
        PocketApiStory(
            // These three properties are required for any valid recommendation.
            title = json.getString(JSON_STORY_TITLE_KEY),
            url = json.getString(JSON_STORY_URL_KEY),
            imageUrl = json.getString(JSON_STORY_IMAGE_URL_KEY),
            // The following three properties are optional.
            publisher = json.tryGetString(JSON_STORY_PUBLISHER_KEY) ?: STRING_NOT_FOUND_DEFAULT_VALUE,
            category = json.tryGetString(JSON_STORY_CATEGORY_KEY) ?: STRING_NOT_FOUND_DEFAULT_VALUE,
            timeToRead = json.tryGetInt(JSON_STORY_TIME_TO_READ_KEY) ?: INT_NOT_FOUND_DEFAULT_VALUE,
        )
    } catch (e: JSONException) {
        null
    }

    companion object {
        @VisibleForTesting const val KEY_ARRAY_ITEMS = "recommendations"

        /**
         * Returns a new instance of [PocketJSONParser].
         */
        fun newInstance(): PocketJSONParser {
            return PocketJSONParser()
        }
    }
}
