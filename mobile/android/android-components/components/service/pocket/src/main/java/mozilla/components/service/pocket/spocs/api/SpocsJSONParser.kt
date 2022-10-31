/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.spocs.api

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

@VisibleForTesting
internal const val JSON_SPOC_CAPS_KEY = "caps"

@VisibleForTesting
internal const val JSON_SPOC_CAPS_LIFETIME_KEY = "lifetime"

@VisibleForTesting
internal const val JSON_SPOC_CAPS_FLIGHT_KEY = "campaign"

@VisibleForTesting
internal const val JSON_SPOC_CAPS_FLIGHT_COUNT_KEY = "count"

@VisibleForTesting
internal const val JSON_SPOC_CAPS_FLIGHT_PERIOD_KEY = "period"
private const val JSON_SPOC_FLIGHT_ID_KEY = "flight_id"
private const val JSON_SPOC_TITLE_KEY = "title"
private const val JSON_SPOC_SPONSOR_KEY = "sponsor"
private const val JSON_SPOC_URL_KEY = "url"
private const val JSON_SPOC_IMAGE_SRC_KEY = "image_src"
private const val JSON_SPOC_SHIM_CLICK_KEY = "click"
private const val JSON_SPOC_SHIM_IMPRESSION_KEY = "impression"
private const val JSON_SPOC_PRIORITY = "priority"

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
            flightId = json.getInt(JSON_SPOC_FLIGHT_ID_KEY),
            title = json.getString(JSON_SPOC_TITLE_KEY),
            sponsor = json.getString(JSON_SPOC_SPONSOR_KEY),
            url = json.getString(JSON_SPOC_URL_KEY),
            imageSrc = json.getString(JSON_SPOC_IMAGE_SRC_KEY),
            shim = jsonToShim(json.getJSONObject(JSON_SPOC_SHIMS_KEY)),
            priority = json.getInt(JSON_SPOC_PRIORITY),
            caps = jsonToCaps(json.getJSONObject(JSON_SPOC_CAPS_KEY)),
        )
    } catch (e: JSONException) {
        null
    }

    private fun jsonToShim(json: JSONObject) = ApiSpocShim(
        click = json.getString(JSON_SPOC_SHIM_CLICK_KEY),
        impression = json.getString(JSON_SPOC_SHIM_IMPRESSION_KEY),
    )

    private fun jsonToCaps(json: JSONObject): ApiSpocCaps {
        val flightCaps = json.getJSONObject(JSON_SPOC_CAPS_FLIGHT_KEY)

        return ApiSpocCaps(
            lifetimeCount = json.getInt(JSON_SPOC_CAPS_LIFETIME_KEY),
            flightCount = flightCaps.getInt(JSON_SPOC_CAPS_FLIGHT_COUNT_KEY),
            flightPeriod = flightCaps.getInt(JSON_SPOC_CAPS_FLIGHT_PERIOD_KEY),
        )
    }
}
