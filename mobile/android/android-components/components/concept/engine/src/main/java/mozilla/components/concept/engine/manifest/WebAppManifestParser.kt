/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.manifest

import android.graphics.Color
import androidx.annotation.ColorInt
import mozilla.components.support.ktx.android.org.json.asSequence
import mozilla.components.support.ktx.android.org.json.tryGetString
import org.json.JSONArray
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
            val shortName = json.tryGetString("short_name")
            val name = json.tryGetString("name") ?: shortName
                ?: return Result.Failure(JSONException("Missing manifest name"))

            Result.Success(WebAppManifest(
                name = name,
                shortName = shortName,
                startUrl = json.getString("start_url"),
                display = parseDisplayMode(json),
                backgroundColor = parseColor(json.tryGetString("background_color")),
                description = json.tryGetString("description"),
                icons = parseIcons(json),
                scope = json.tryGetString("scope"),
                themeColor = parseColor(json.tryGetString("theme_color")),
                dir = parseTextDirection(json),
                lang = json.tryGetString("lang"),
                orientation = parseOrientation(json)
            ))
        } catch (e: JSONException) {
            Result.Failure(e)
        }
    }

    /**
     * Parses the provided JSON and returns a [WebAppManifest] (wrapped in [Result.Success] if parsing was successful.
     * Otherwise [Result.Failure].
     */
    fun parse(json: String): Result {
        return try {
            parse(JSONObject(json))
        } catch (e: JSONException) {
            Result.Failure(e)
        }
    }

    fun serialize(manifest: WebAppManifest) = JSONObject().apply {
        put("name", manifest.name)
        putOpt("short_name", manifest.shortName)
        put("start_url", manifest.startUrl)
        putOpt("display", serializeEnumName(manifest.display.name))
        putOpt("background_color", serializeColor(manifest.backgroundColor))
        putOpt("description", manifest.description)
        putOpt("icons", serializeIcons(manifest.icons))
        putOpt("scope", manifest.scope)
        putOpt("theme_color", serializeColor(manifest.themeColor))
        putOpt("dir", serializeEnumName(manifest.dir.name))
        putOpt("lang", manifest.lang)
        putOpt("orientation", serializeEnumName(manifest.orientation.name))
    }
}

private val whitespace = "\\s+".toRegex()

private fun parseDisplayMode(json: JSONObject): WebAppManifest.DisplayMode {
    return when (json.optString("display")) {
        "standalone" -> WebAppManifest.DisplayMode.STANDALONE
        "fullscreen" -> WebAppManifest.DisplayMode.FULLSCREEN
        "minimal-ui" -> WebAppManifest.DisplayMode.MINIMAL_UI
        "browser" -> WebAppManifest.DisplayMode.BROWSER
        else -> WebAppManifest.DisplayMode.BROWSER
    }
}

@ColorInt
private fun parseColor(color: String?): Int? {
    if (color == null || !color.startsWith("#")) {
        return null
    }

    return try {
        Color.parseColor(color)
    } catch (e: IllegalArgumentException) {
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
    val sizes = json.tryGetString("sizes") ?: return emptyList()

    return sizes
        .split(whitespace)
        .mapNotNull { Size.parse(it) }
}

private fun parsePurposes(json: JSONObject): Set<WebAppManifest.Icon.Purpose> =
    json.tryGetString("purpose").orEmpty()
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

private fun serializeEnumName(name: String) = name.toLowerCase().replace('_', '-')

@Suppress("MagicNumber")
private fun serializeColor(color: Int?): String? = color?.let {
    String.format("#%06X", 0xFFFFFF and color)
}

private fun serializeIcons(icons: List<WebAppManifest.Icon>): JSONArray {
    val list = icons.map { icon ->
        JSONObject().apply {
            put("src", icon.src)
            put("sizes", icon.sizes.joinToString(" ") { it.toString() })
            putOpt("type", icon.type)
            put("purpose", icon.purpose.joinToString(" ") { serializeEnumName(it.name) })
        }
    }
    return JSONArray(list)
}
