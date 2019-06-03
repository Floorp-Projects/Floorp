/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.extension

import mozilla.components.browser.icons.IconRequest
import mozilla.components.concept.engine.manifest.Size
import mozilla.components.support.base.log.logger.Logger
import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject

private val typeMap: Map<String, IconRequest.Resource.Type> = mutableMapOf(
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
    "msapplication-TileImage" to IconRequest.Resource.Type.MICROSOFT_TILE
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
    val array = JSONArray()

    for (resource in this) {
        val item = JSONObject()

        item.put("href", resource.url)

        resource.mimeType?.let { item.put("mimeType", it) }

        item.put("type", typeMap.reverseLookup(resource.type))

        val sizeArray = JSONArray()
        resource.sizes.forEach { size ->
            sizeArray.put("${size.width}x${size.height}")
        }
        item.put("sizes", sizeArray)

        array.put(item)
    }

    return array
}

internal fun JSONObject.toIconRequest(): IconRequest? {
    return try {
        val url = getString("url")

        IconRequest(url, resources = getJSONArray("icons").toIconResources())
    } catch (e: JSONException) {
        Logger.warn("Could not parse message from icons extensions", e)
        null
    }
}

internal fun JSONArray.toIconResources(): List<IconRequest.Resource> {
    val resources = mutableListOf<IconRequest.Resource>()

    for (i in 0 until length()) {
        val resource = getJSONObject(i).toIconResource()
        resource?.let { resources.add(it) }
    }

    return resources
}

private fun JSONObject.toIconResource(): IconRequest.Resource? {
    try {
        val url = getString("href")
        val type = typeMap[getString("type")]
            ?: return null
        val sizes = optJSONArray("sizes").toResourceSizes()
        val mimeType = optString("mimeType", null)

        return IconRequest.Resource(url, type, sizes, if (mimeType.isNullOrEmpty()) null else mimeType)
    } catch (e: JSONException) {
        Logger.warn("Could not parse message from icons extensions", e)
        return null
    }
}

private fun JSONArray?.toResourceSizes(): List<Size> {
    this ?: return emptyList()

    return try {
        val sizes = mutableListOf<Size>()

        for (i in 0 until length()) {
            val raw = getString(i)
            Size.parse(raw)?.let { sizes.add(it) }
        }

        sizes
    } catch (e: JSONException) {
        Logger.warn("Could not parse message from icons extensions", e)
        emptyList()
    } catch (e: NumberFormatException) {
        Logger.warn("Could not parse message from icons extensions", e)
        emptyList()
    }
}
