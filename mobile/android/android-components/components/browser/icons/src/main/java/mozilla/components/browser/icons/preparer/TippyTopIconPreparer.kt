/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.preparer

import android.content.Context
import android.content.res.AssetManager
import android.net.Uri
import androidx.core.net.toUri
import mozilla.components.browser.icons.IconRequest
import mozilla.components.support.base.log.Log
import mozilla.components.support.ktx.android.net.hostWithoutCommonPrefixes
import mozilla.components.support.ktx.android.net.isHttpOrHttps
import mozilla.components.support.ktx.android.org.json.asSequence
import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject

private const val LIST_FILE_PATH = "mozac.browser.icons/icons-top200.json"

// Make sure domain added here have the corresponding image_url in icons-top200.json
private val commonDomain = listOf("wikipedia.org")

/**
 * Returns the host's common domain if found, else null is returned
 */
private val Uri.hostWithCommonDomain: String?
    get() {
        val host = host ?: return null
        for (domain in commonDomain) {
            if (host.endsWith(domain)) return domain
        }
        return null
    }

/**
 * [IconPreprarer] implementation that looks up the host in our "tippy top" list. If it can find a match then it inserts
 * the icon URL into the request.
 *
 * The "tippy top" list is a list of "good" icons for top pages maintained by Mozilla:
 * https://github.com/mozilla/tippy-top-sites
 */
class TippyTopIconPreparer(
    assetManager: AssetManager,
) : IconPreprarer {
    private val iconMap: Map<String, String> by lazy { parseList(assetManager) }

    override fun prepare(context: Context, request: IconRequest): IconRequest {
        val uri = request.url.toUri()
        if (!uri.isHttpOrHttps) {
            return request
        }

        val host = uri.hostWithCommonDomain ?: uri.hostWithoutCommonPrefixes

        return if (host != null && iconMap.containsKey(host)) {
            val resource = IconRequest.Resource(
                url = iconMap.getValue(host),
                type = IconRequest.Resource.Type.TIPPY_TOP,
            )

            request.copy(resources = request.resources + resource)
        } else {
            request
        }
    }
}

private fun parseList(assetManager: AssetManager): Map<String, String> = try {
    JSONArray(assetManager.open(LIST_FILE_PATH).bufferedReader().readText())
        .asSequence()
        .flatMap { entry ->
            val json = entry as JSONObject
            val domains = json.getJSONArray("domains")
            val iconUrl = json.getString("image_url")

            domains.asSequence().map { domain -> Pair(domain.toString(), iconUrl) }
        }
        .toMap()
} catch (e: JSONException) {
    Log.log(
        priority = Log.Priority.ERROR,
        tag = "TippyTopIconPreparer",
        message = "Could not load tippy top list from assets",
        throwable = e,
    )
    emptyMap()
}
