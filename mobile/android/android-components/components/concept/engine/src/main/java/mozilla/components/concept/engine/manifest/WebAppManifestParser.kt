/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.manifest

import mozilla.components.support.ktx.android.org.json.asSequence
import org.json.JSONException
import org.json.JSONObject

/**
 * Parser for constructing a [WebAppManifest] from JSON.
 */
class WebAppManifestParser {
    /**
     * A parsing result.
     */
    sealed class Result {
        /**
         * The JSON was parsed successful.
         *
         * @property manifest The parsed [WebAppManifest] object.
         */
        data class Success(val manifest: WebAppManifest) : Result()

        /**
         * Parsing the JSON failed.
         *
         * @property exception The exception that was thrown while parsing the manifest.
         */
        data class Failure(val exception: JSONException) : Result()
    }

    /**
     * Parses the provided JSON and returns a [WebAppManifest] (wrapped in [Result.Success] if parsing was successful.
     * Otherwise [Result.Failure].
     */
    fun parse(json: JSONObject): Result {
        return try {
            Result.Success(WebAppManifest(
                name = json.getString("name"),
                shortName = json.optString("short_name", null),
                startUrl = json.getString("start_url"),
                display = getDisplayMode(json),
                backgroundColor = parseColor(json.optString("background_color", null)),
                description = json.optString("description", null),
                icons = parseIcons(json),
                scope = json.optString("scope", null),
                themeColor = parseColor(json.optString("theme_color", null)),
                dir = parseTextDirection(json),
                lang = json.optString("lang", null),
                orientation = parseOrientation(json)
            ))
        } catch (e: JSONException) {
            Result.Failure(e)
        }
    }
}

private val whitespace = "\\s+".toRegex()

private fun getDisplayMode(json: JSONObject): WebAppManifest.DisplayMode {
    return when (json.optString("display")) {
        "standalone" -> WebAppManifest.DisplayMode.STANDALONE
        "fullscreen" -> WebAppManifest.DisplayMode.FULLSCREEN
        "minimal-ui" -> WebAppManifest.DisplayMode.MINIMAL_UI
        "browser" -> WebAppManifest.DisplayMode.BROWSER
        else -> WebAppManifest.DisplayMode.BROWSER
    }
}

@Suppress("MagicNumber")
private fun parseColor(color: String?): Int? {
    if (color == null || !color.startsWith("#")) {
        return null
    }

    return try {
        Integer.parseInt(color.substring(1), 16)
    } catch (e: NumberFormatException) {
        null
    }
}

private fun parseIcons(json: JSONObject): List<WebAppManifest.Icon> {
    val array = json.optJSONArray("icons") ?: return emptyList()

    return array
        .asSequence()
        .map { it as JSONObject }
        .map { obj ->
            WebAppManifest.Icon(
                src = obj.getString("src"),
                sizes = parseIconSizes(obj),
                type = obj.optString("type", null),
                purpose = parsePurposes(obj)
            )
        }
        .toList()
}

private fun parseIconSizes(json: JSONObject): List<Size> {
    val sizes = json.optString("sizes") ?: return emptyList()

    return sizes
        .split(whitespace)
        .mapNotNull { Size.parse(it) }
}

private fun parsePurposes(json: JSONObject): Set<WebAppManifest.Icon.Purpose> {
    val purposes = json.optString("purpose") ?: return emptySet()

    return purposes
        .split(whitespace)
        .mapNotNull {
            when (it.toLowerCase()) {
                "badge" -> WebAppManifest.Icon.Purpose.BADGE
                "maskable" -> WebAppManifest.Icon.Purpose.MASKABLE
                "any" -> WebAppManifest.Icon.Purpose.ANY
                else -> null
            }
        }
        .toSet()
        .ifEmpty { setOf(WebAppManifest.Icon.Purpose.ANY) }
}

private fun parseTextDirection(json: JSONObject): WebAppManifest.TextDirection {
    return when (json.optString("dir")) {
        "ltr" -> WebAppManifest.TextDirection.LTR
        "rtl" -> WebAppManifest.TextDirection.RTL
        "auto" -> WebAppManifest.TextDirection.AUTO
        else -> WebAppManifest.TextDirection.AUTO
    }
}

private fun parseOrientation(json: JSONObject) = when (json.optString("orientation")) {
    "any" -> WebAppManifest.Orientation.ANY
    "natural" -> WebAppManifest.Orientation.NATURAL
    "landscape" -> WebAppManifest.Orientation.LANDSCAPE
    "portrait" -> WebAppManifest.Orientation.PORTRAIT
    "portrait-primary" -> WebAppManifest.Orientation.PORTRAIT_PRIMARY
    "portrait-secondary" -> WebAppManifest.Orientation.PORTRAIT_SECONDARY
    "landscape-primary" -> WebAppManifest.Orientation.LANDSCAPE_PRIMARY
    "landscape-secondary" -> WebAppManifest.Orientation.LANDSCAPE_SECONDARY
    else -> WebAppManifest.Orientation.ANY
}
