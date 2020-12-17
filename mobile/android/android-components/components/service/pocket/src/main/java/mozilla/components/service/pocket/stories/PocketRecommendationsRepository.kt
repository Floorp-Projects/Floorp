/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.stories

import android.content.Context
import android.util.AtomicFile
import androidx.annotation.VisibleForTesting
import mozilla.components.service.pocket.PocketRecommendedStory
import mozilla.components.support.ktx.android.org.json.mapNotNull
import mozilla.components.support.ktx.util.readAndDeserialize
import mozilla.components.support.ktx.util.writeString
import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject
import java.io.File

@VisibleForTesting internal const val STORIES_FILE_NAME = "pocket_recommended.stories"
@VisibleForTesting internal const val JSON_KEY_ARRAY_STORIES = "list"

/**
 * Wrapper over our local database.
 * Allows for easy CRUD operations.
 */
internal class PocketRecommendationsRepository(private val context: Context) {
    private val diskCacheLock = Any()

    /**
     * Returns the current locally persisted list of Pocket recommended stories.
     */
    suspend fun getPocketRecommendedStories(): List<PocketRecommendedStory> {
        synchronized(diskCacheLock) {
            return getAtomicStoriesFile().readAndDeserialize {
                JSONObject(it).toPocketRecommendedStories()
            } ?: emptyList()
        }
    }

    /**
     * Replaces the current list of locally persisted Pocket recommended stories.
     *
     * @param stories A String representation of a JSONObject with Pocket recommended stories to persist locally.
     */
    suspend fun addAllPocketPocketRecommendedStories(
        stories: String
    ) = synchronized(diskCacheLock) {
        getAtomicStoriesFile().writeString { stories }
    }

    @VisibleForTesting
    internal fun getAtomicStoriesFile() = AtomicFile(getStoriesFile())

    @VisibleForTesting
    internal fun getStoriesFile() = File(context.cacheDir, STORIES_FILE_NAME)
}

/**
 * Returns a list of Pocket stories inferred from this JSON object.
 * Assumes the JSON object follows the structure from "https://getpocket.cdn.mozilla.net/v3/firefox"
 * Invalid entries will be dropped, as such the returned list may be empty.
 */
@VisibleForTesting
internal fun JSONObject.toPocketRecommendedStories(): List<PocketRecommendedStory> {
    val storiesJSON = getJSONArray(JSON_KEY_ARRAY_STORIES)
    return storiesJSON.mapNotNull(JSONArray::getJSONObject) { it.toPocketRecommendedStory() }
}

/**
 * Do a 1-1 mapping of a JSON object representing a Pocket story to a PocketRecommendedStory.
 * The result may be null if the JSON object doest not respect the expected structure.
 */
@VisibleForTesting
internal fun JSONObject.toPocketRecommendedStory(): PocketRecommendedStory? = try {
    PocketRecommendedStory(
        id = getLong("id"),
        url = getString("url"),
        domain = getString("domain"),
        title = getString("title"),
        excerpt = getString("excerpt"),
        imageSrc = getString("image_src"),
        publishedTimestamp = getString("published_timestamp"),
        dedupeUrl = getString("dedupe_url"),
        sortId = getInt("sort_id")
    )
} catch (e: JSONException) {
    null
}
