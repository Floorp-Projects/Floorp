/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package mozilla.components.concept.engine.manifest

import mozilla.components.support.ktx.android.org.json.asSequence
import mozilla.components.support.ktx.android.org.json.tryGetString
import org.json.JSONArray
import org.json.JSONObject

private val whitespace = "\\s+".toRegex()

/**
 * Parses the icons array from a web app manifest.
 */
internal fun parseIcons(json: JSONObject): List<WebAppManifest.Icon> {
    val array = json.optJSONArray("icons") ?: return emptyList()

    return array
        .asSequence { i -> getJSONObject(i) }
        .mapNotNull { obj ->
            val purpose = parsePurposes(obj).ifEmpty {
                return@mapNotNull null
            }
            WebAppManifest.Icon(
                src = obj.getString("src"),
                sizes = parseIconSizes(obj),
                type = obj.tryGetString("type"),
                purpose = purpose
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

@Suppress("ComplexMethod")
private fun parsePurposes(json: JSONObject): Set<WebAppManifest.Icon.Purpose> {
    // "purpose" is normally a space-separated string, but Gecko current returns an array instead.
    val purposeRaw = if (json.has("purpose")) {
        json.get("purpose")
    } else {
        null
    }

    val purpose = when (purposeRaw) {
        is String -> purposeRaw.split(whitespace).asSequence()
        is JSONArray -> purposeRaw.asSequence { i -> getString(i) }
        else -> return setOf(WebAppManifest.Icon.Purpose.ANY)
    }

    return purpose
        .mapNotNull {
            when (it.toLowerCase()) {
                "badge" -> WebAppManifest.Icon.Purpose.BADGE
                "maskable" -> WebAppManifest.Icon.Purpose.MASKABLE
                "any" -> WebAppManifest.Icon.Purpose.ANY
                else -> null
            }
        }
        .toSet()
}

internal fun serializeEnumName(name: String) = name.toLowerCase().replace('_', '-')

internal fun serializeIcons(icons: List<WebAppManifest.Icon>): JSONArray {
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
