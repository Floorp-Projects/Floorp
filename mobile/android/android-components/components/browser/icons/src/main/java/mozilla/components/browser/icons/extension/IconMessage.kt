/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.extension

import mozilla.components.browser.icons.IconRequest
import mozilla.components.concept.engine.manifest.Size
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.org.json.asSequence
import mozilla.components.support.ktx.android.org.json.toJSONArray
import mozilla.components.support.ktx.android.org.json.tryGetString
import mozilla.components.support.ktx.kotlin.sanitizeURL
import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject

private val typeMap: Map<String, IconRequest.Resource.Type> = mutableMapOf(
    "manifest" to IconRequest.Resource.Type.MANIFEST_ICON,
    "icon" to IconRequest.Resource.Type.FAVICON,
    "shortcut icon" to IconRequest.Resource.Type.FAVICON,
    "fluid-icon" to IconRequest.Resource.Type.FLUID_ICON,
    "apple-touch-icon" to IconRequest.Resource.Type.APPLE_TOUCH_ICON,
    "image_src" to IconRequest.Resource.Type.IMAGE_SRC,
    "apple-touch-icon image_src" to IconRequest.Resource.Type.APPLE_TOUCH_ICON,
    "apple-touch-icon-precomposed" to IconRequest.Resource.Type.APPLE_TOUCH_ICON,
    "og:image" to IconRequest.Resource.Type.OPENGRAPH,
    "og:image:url" to IconRequest.Resource.Type.OPENGRAPH,
    "og:image:secure_url" to IconRequest.Resource.Type.OPENGRAPH,
    "twitter:image" to IconRequest.Resource.Type.TWITTER,
    "msapplication-TileImage" to IconRequest.Resource.Type.MICROSOFT_TILE,
)

private fun Map<String, IconRequest.Resource.Type>.reverseLookup(type: IconRequest.Resource.Type): String {
    forEach { (value, currentType) ->
        if (currentType == type) {
            return value
        }
    }

    throw IllegalArgumentException("Unknown type: $type")
}

internal fun List<IconRequest.Resource>.toJSON(): JSONArray {
    return mapNotNull { resource ->
        if (resource.type == IconRequest.Resource.Type.TIPPY_TOP) {
            // Ignore the URLs coming from the "tippy top" list.
            return@mapNotNull null
        }

        JSONObject().apply {
            put("href", resource.url)

            resource.mimeType?.let { put("mimeType", it) }

            put("type", typeMap.reverseLookup(resource.type))

            val sizeArray = resource.sizes.map { size -> size.toString() }.toJSONArray()
            put("sizes", sizeArray)

            put("maskable", resource.maskable)
        }
    }.toJSONArray()
}

internal fun JSONObject.toIconRequest(isPrivate: Boolean): IconRequest? {
    return try {
        val url = getString("url")

        IconRequest(url, isPrivate = isPrivate, resources = getJSONArray("icons").toIconResources())
    } catch (e: JSONException) {
        Logger.warn("Could not parse message from icons extensions", e)
        null
    }
}

internal fun JSONArray.toIconResources(): List<IconRequest.Resource> {
    return asSequence { i -> getJSONObject(i) }
        .mapNotNull { it.toIconResource() }
        .toList()
}

private fun JSONObject.toIconResource(): IconRequest.Resource? {
    try {
        val url = getString("href")
        val type = typeMap[getString("type")] ?: return null
        val sizes = optJSONArray("sizes").toResourceSizes()
        val mimeType = tryGetString("mimeType")
        val maskable = optBoolean("maskable", false)

        return IconRequest.Resource(
            url = url.sanitizeURL(),
            type = type,
            sizes = sizes,
            mimeType = if (mimeType.isNullOrEmpty()) null else mimeType,
            maskable = maskable,
        )
    } catch (e: JSONException) {
        Logger.warn("Could not parse message from icons extensions", e)
        return null
    }
}

private fun JSONArray?.toResourceSizes(): List<Size> {
    val array = this ?: return emptyList()

    return try {
        array.asSequence { i -> getString(i) }
            .mapNotNull { raw -> Size.parse(raw) }
            .toList()
    } catch (e: JSONException) {
        Logger.warn("Could not parse message from icons extensions", e)
        emptyList()
    } catch (e: NumberFormatException) {
        Logger.warn("Could not parse message from icons extensions", e)
        emptyList()
    }
}
