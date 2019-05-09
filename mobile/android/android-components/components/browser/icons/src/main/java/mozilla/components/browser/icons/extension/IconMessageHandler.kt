/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.extension

import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.icons.IconRequest
import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.webextension.MessageHandler
import mozilla.components.support.base.log.logger.Logger
import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject
import java.lang.IllegalStateException

/**
 * [MessageHandler] implementation that receives messages from the icons web extensions and performs icon loads.
 */
internal class IconMessageHandler(
    private val session: Session,
    private val icons: BrowserIcons
) : MessageHandler {
    private val scope = CoroutineScope(Dispatchers.Main)

    @VisibleForTesting(otherwise = VisibleForTesting.NONE) // This only exists so that we can wait in tests.
    internal var lastJob: Job? = null

    override fun onMessage(message: Any, source: EngineSession?): Any {
        if (message is JSONObject) {
            message.toIconRequest()?.let { loadRequest(it) }
        } else {
            throw IllegalStateException("Received unexpected message: $message")
        }

        // Needs to return something that is not null and not Unit:
        // https://github.com/mozilla-mobile/android-components/issues/2969
        return ""
    }

    private fun loadRequest(request: IconRequest) {
        lastJob = scope.launch {
            val icon = icons.loadIcon(request).await()

            if (session.url == request.url) {
                // Only update the icon of the session if we are still on this page. The user may have navigated
                // away by the time the icon is loaded.
                session.icon = icon.bitmap
            }
        }
    }
}

internal fun JSONObject.toIconRequest(): IconRequest? {
    return try {
        val url = getString("url")

        val resources = mutableListOf<IconRequest.Resource>()

        val icons = getJSONArray("icons")
        for (i in 0 until icons.length()) {
            val resource = icons.getJSONObject(i).toIconResource()
            resource?.let { resources.add(it) }
        }

        IconRequest(url, resources = resources)
    } catch (e: JSONException) {
        Logger.warn("Could not parse message from icons extensions", e)
        null
    }
}

private fun JSONObject.toIconResource(): IconRequest.Resource? {
    try {
        val url = getString("href")
        val type = getString("type").toResourceType()
            ?: return null
        val sizes = optJSONArray("sizes").toResourceSizes()
        val mimeType = optString("mimeType", null)

        return IconRequest.Resource(url, type, sizes, if (mimeType.isNullOrEmpty()) null else mimeType)
    } catch (e: JSONException) {
        Logger.warn("Could not parse message from icons extensions", e)
        return null
    }
}

@Suppress("ComplexMethod")
private fun String.toResourceType(): IconRequest.Resource.Type? {
    return when (this) {
        "icon" -> IconRequest.Resource.Type.FAVICON
        "shortcut icon" -> IconRequest.Resource.Type.FAVICON
        "fluid-icon" -> IconRequest.Resource.Type.FLUID_ICON
        "apple-touch-icon" -> IconRequest.Resource.Type.APPLE_TOUCH_ICON
        "image_src" -> IconRequest.Resource.Type.IMAGE_SRC
        "apple-touch-icon image_src" -> IconRequest.Resource.Type.APPLE_TOUCH_ICON
        "apple-touch-icon-precomposed" -> IconRequest.Resource.Type.APPLE_TOUCH_ICON
        "og:image" -> IconRequest.Resource.Type.OPENGRAPH
        "og:image:url" -> IconRequest.Resource.Type.OPENGRAPH
        "og:image:secure_url" -> IconRequest.Resource.Type.OPENGRAPH
        "twitter:image" -> IconRequest.Resource.Type.TWITTER
        "msapplication-TileImage" -> IconRequest.Resource.Type.MICROSOFT_TILE
        else -> null
    }
}

@Suppress("ReturnCount")
private fun JSONArray?.toResourceSizes(): List<IconRequest.Resource.Size> {
    if (this == null) {
        return emptyList()
    }

    try {
        val sizes = mutableListOf<IconRequest.Resource.Size>()

        for (i in 0 until length()) {
            val size = getString(i).split("x")
            if (size.size != 2) {
                continue
            }

            val width = size[0].toInt()
            val height = size[1].toInt()

            sizes.add(IconRequest.Resource.Size(width, height))
        }

        return sizes
    } catch (e: JSONException) {
        Logger.warn("Could not parse message from icons extensions", e)
        return emptyList()
    } catch (e: NumberFormatException) {
        Logger.warn("Could not parse message from icons extensions", e)
        return emptyList()
    }
}
