/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.wallpapers

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import org.json.JSONArray
import org.json.JSONObject
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.utils.toHexColor
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * Utility class for downloading wallpaper metadata from the remote server.
 *
 * @param client The client that will be used to fetch metadata.
 */
class WallpaperMetadataFetcher(
    private val client: Client,
) {
    private val metadataUrl = BuildConfig.WALLPAPER_URL.substringBefore("android") +
        "metadata/v$currentJsonVersion/wallpapers.json"

    /**
     * Downloads the list of wallpapers from the remote source. Failures will return an empty list.
     */
    suspend fun downloadWallpaperList(): List<Wallpaper> = withContext(Dispatchers.IO) {
        Result.runCatching {
            val request = Request(url = metadataUrl, method = Request.Method.GET)
            val response = client.fetch(request)
            response.body.useBufferedReader {
                val json = it.readText()
                JSONObject(json).parseAsWallpapers()
            }
        }.getOrElse { listOf() }
    }

    private fun JSONObject.parseAsWallpapers(): List<Wallpaper> =
        with(getJSONArray("collections")) {
            (0 until length()).map { index ->
                getJSONObject(index).toCollectionOfWallpapers()
            }.flatten()
        }

    private fun JSONObject.toCollectionOfWallpapers(): List<Wallpaper> {
        val collectionId = getString("id")
        val heading = optStringOrNull("heading")
        val description = optStringOrNull("description")
        val availableLocales = optJSONArray("available-locales")?.getAvailableLocales()
        val availabilityRange = optJSONObject("availability-range")?.getAvailabilityRange()
        val learnMoreUrl = optStringOrNull("learn-more-url")
        val collection = Wallpaper.Collection(
            name = collectionId,
            heading = heading,
            description = description,
            availableLocales = availableLocales,
            startDate = availabilityRange?.first,
            endDate = availabilityRange?.second,
            learnMoreUrl = learnMoreUrl,
        )
        return getJSONArray("wallpapers").toWallpaperList(collection)
    }

    private fun JSONArray.getAvailableLocales(): List<String>? =
        (0 until length()).map { getString(it) }

    private fun JSONObject.getAvailabilityRange(): Pair<Date, Date>? {
        val formatter = SimpleDateFormat("yyyy-MM-dd", Locale.US)
        return Result.runCatching {
            formatter.parse(getString("start"))!! to formatter.parse(getString("end"))!!
        }.getOrNull()
    }

    private fun JSONArray.toWallpaperList(collection: Wallpaper.Collection): List<Wallpaper> =
        (0 until length()).map { index ->
            with(getJSONObject(index)) {
                Wallpaper(
                    name = getString("id"),
                    textColor = getArgbValueAsLong("text-color"),
                    cardColorLight = getArgbValueAsLong("card-color-light"),
                    cardColorDark = getArgbValueAsLong("card-color-dark"),
                    collection = collection,
                    thumbnailFileState = Wallpaper.ImageFileState.Unavailable,
                    assetsFileState = Wallpaper.ImageFileState.Unavailable,
                )
            }
        }

    /**
     * Normally, if a field is specified in json as null, then optString will return it as "null". If
     * a field is missing completely, optString will return "". This will correctly return null in
     * both those cases so that optional properties are marked as missing.
     */
    private fun JSONObject.optStringOrNull(propName: String) = optString(propName).takeIf {
        it != "null" && it.isNotEmpty()
    }

    /**
     * The wallpaper metadata has 6 digit hex color codes for compatibility with iOS. Since Android
     * expects 8 digit ARBG values, we prepend FF for the "fully visible" version of the color
     * listed in the metadata.
     */
    private fun JSONObject.getArgbValueAsLong(propName: String): Long = "FF${getString(propName)}"
        .toHexColor()

    companion object {
        internal const val currentJsonVersion = 1
    }
}
