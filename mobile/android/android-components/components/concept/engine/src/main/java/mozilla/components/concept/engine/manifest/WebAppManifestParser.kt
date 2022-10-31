/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.manifest

import android.graphics.Color
import androidx.annotation.ColorInt
import mozilla.components.concept.engine.manifest.parser.ShareTargetParser
import mozilla.components.concept.engine.manifest.parser.parseIcons
import mozilla.components.concept.engine.manifest.parser.serializeEnumName
import mozilla.components.concept.engine.manifest.parser.serializeIcons
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
     *
     * Gecko performs some initial parsing on the Web App Manifest, so the [JSONObject] we work with
     * does not match what was originally provided by the website. Gecko:
     * - Changes relative URLs to be absolute
     * - Changes some space-separated strings into arrays (purpose, sizes)
     * - Changes colors to follow Android format (#AARRGGBB)
     * - Removes invalid enum values (ie display: halfscreen)
     * - Ensures display, dir, start_url, and scope always have a value
     * - Trims most strings (name, short_name, ...)
     * See https://searchfox.org/mozilla-central/source/dom/manifest/ManifestProcessor.jsm
     */
    fun parse(json: JSONObject): Result {
        return try {
            val shortName = json.tryGetString("short_name")
            val name = json.tryGetString("name") ?: shortName
                ?: return Result.Failure(JSONException("Missing manifest name"))

            Result.Success(
                WebAppManifest(
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
                    orientation = parseOrientation(json),
                    relatedApplications = parseRelatedApplications(json),
                    preferRelatedApplications = json.optBoolean("prefer_related_applications", false),
                    shareTarget = ShareTargetParser.parse(json.optJSONObject("share_target")),
                ),
            )
        } catch (e: JSONException) {
            Result.Failure(e)
        }
    }

    /**
     * Parses the provided JSON and returns a [WebAppManifest] (wrapped in [Result.Success] if parsing was successful.
     * Otherwise [Result.Failure].
     */
    fun parse(json: String) = try {
        parse(JSONObject(json))
    } catch (e: JSONException) {
        Result.Failure(e)
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
        putOpt("orientation", serializeEnumName(manifest.orientation.name))
        put("related_applications", serializeRelatedApplications(manifest.relatedApplications))
        put("prefer_related_applications", manifest.preferRelatedApplications)
        putOpt("share_target", ShareTargetParser.serialize(manifest.shareTarget))
    }
}

/**
 * Returns the encapsulated value if this instance represents success or `null` if it is failure.
 */
fun WebAppManifestParser.Result.getOrNull(): WebAppManifest? = when (this) {
    is WebAppManifestParser.Result.Success -> manifest
    is WebAppManifestParser.Result.Failure -> null
}

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

private fun parseRelatedApplications(json: JSONObject): List<WebAppManifest.ExternalApplicationResource> {
    val array = json.optJSONArray("related_applications") ?: return emptyList()

    return array
        .asSequence { i -> getJSONObject(i) }
        .mapNotNull { app -> parseRelatedApplication(app) }
        .toList()
}

private fun parseRelatedApplication(app: JSONObject): WebAppManifest.ExternalApplicationResource? {
    val platform = app.tryGetString("platform")
    val url = app.tryGetString("url")
    val id = app.tryGetString("id")
    return if (platform != null && (url != null || id != null)) {
        WebAppManifest.ExternalApplicationResource(
            platform = platform,
            url = url,
            id = id,
            minVersion = app.tryGetString("min_version"),
            fingerprints = parseFingerprints(app),
        )
    } else {
        null
    }
}

private fun parseFingerprints(app: JSONObject): List<WebAppManifest.ExternalApplicationResource.Fingerprint> {
    val array = app.optJSONArray("fingerprints") ?: return emptyList()

    return array
        .asSequence { i -> getJSONObject(i) }
        .map {
            WebAppManifest.ExternalApplicationResource.Fingerprint(
                type = it.getString("type"),
                value = it.getString("value"),
            )
        }
        .toList()
}

@Suppress("MagicNumber")
private fun serializeColor(color: Int?): String? = color?.let {
    String.format("#%06X", 0xFFFFFF and color)
}

private fun serializeRelatedApplications(
    relatedApplications: List<WebAppManifest.ExternalApplicationResource>,
): JSONArray {
    val list = relatedApplications.map { app ->
        JSONObject().apply {
            put("platform", app.platform)
            putOpt("url", app.url)
            putOpt("id", app.id)
            putOpt("min_version", app.minVersion)
            put("fingerprints", serializeFingerprints(app.fingerprints))
        }
    }
    return JSONArray(list)
}

private fun serializeFingerprints(
    fingerprints: List<WebAppManifest.ExternalApplicationResource.Fingerprint>,
): JSONArray {
    val list = fingerprints.map {
        JSONObject().apply {
            put("type", it.type)
            put("value", it.value)
        }
    }
    return JSONArray(list)
}
