/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.spocs

import androidx.annotation.VisibleForTesting
import mozilla.components.service.pocket.logger
import mozilla.components.support.ktx.android.org.json.mapNotNull
import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject

@VisibleForTesting
internal const val KEY_ARRAY_SPOCS = "spocs"
@VisibleForTesting
internal const val JSON_SPOC_SHIMS_KEY = "shim"
private const val JSON_SPOC_TITLE_KEY = "title"
private const val JSON_SPOC_SPONSOR_KEY = "sponsor"
private const val JSON_SPOC_URL_KEY = "url"
private const val JSON_SPOC_IMAGE_SRC_KEY = "image_src"
private const val JSON_SPOC_SHIM_CLICK_KEY = "click"
private const val JSON_SPOC_SHIM_IMPRESSION_KEY = "impression"

/**
 * Holds functions that parse the JSON returned by the Pocket API and converts them to more usable Kotlin types.
 */
internal object SpocsJSONParser {
    /**
     * @return The stories, removing entries that are invalid, or null on error; the list will never be empty.
     */
    fun jsonToSpocs(json: String): List<ApiSpoc>? = try {
        val rawJSON = JSONObject(json)
        val spocsJSON = rawJSON.getJSONArray(KEY_ARRAY_SPOCS)
        val spocs = spocsJSON.mapNotNull(JSONArray::getJSONObject) { jsonToSpoc(it) }

        // We return null, rather than the empty list, because devs might forget to check an empty list.
        spocs.ifEmpty { null }
    } catch (e: JSONException) {
        logger.warn("invalid JSON from the SPOCS endpoint", e)
        null
    }

    private fun jsonToSpoc(json: JSONObject): ApiSpoc? = try {
        ApiSpoc(
            title = json.getString(JSON_SPOC_TITLE_KEY),
            sponsor = json.getString(JSON_SPOC_SPONSOR_KEY),
            url = json.getString(JSON_SPOC_URL_KEY),
            imageSrc = json.getString(JSON_SPOC_IMAGE_SRC_KEY),
            shim = jsonToShim(json.getJSONObject(JSON_SPOC_SHIMS_KEY))
        )
    } catch (e: JSONException) {
        null
    }

    private fun jsonToShim(json: JSONObject) = ApiSpocShim(
        click = json.getString(JSON_SPOC_SHIM_CLICK_KEY),
        impression = json.getString(JSON_SPOC_SHIM_IMPRESSION_KEY)
    )
}
