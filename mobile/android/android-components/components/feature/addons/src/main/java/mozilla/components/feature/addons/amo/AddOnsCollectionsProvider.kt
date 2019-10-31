/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package mozilla.components.feature.addons.amo

import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.isSuccess
import mozilla.components.feature.addons.AddOnsProvider
import mozilla.components.feature.addons.AddOn
import org.json.JSONArray
import org.json.JSONObject
import java.io.IOException

internal const val API_VERSION = "api/v4"
internal const val DEFAULT_SERVER_URL = "https://addons.mozilla.org"
internal const val DEFAULT_COLLECTION_NAME = "7e8d6dc651b54ab385fb8791bf9dac"

/**
 * Provide access to the collections AMO API.
 * https://addons-server.readthedocs.io/en/latest/topics/api/collections.html
 *
 * @property serverURL The url of the endpoint to interact with e.g production, staging
 * or testing. Defaults to [DEFAULT_SERVER_URL].
 * @property collectionName The name of the collection to access, defaults
 * to [DEFAULT_COLLECTION_NAME].
 * @property client A reference of [Client] for interacting with the AMO HTTP api.
 */
class AddOnsCollectionsProvider(
    private val serverURL: String = DEFAULT_SERVER_URL,
    private val collectionName: String = DEFAULT_COLLECTION_NAME,
    private val client: Client
) : AddOnsProvider {

    /**
     * Interacts with the collections endpoint to provide a list of available add-ons.
     * @throws IOException if the request could not be executed due to cancellation,
     * a connectivity problem or a timeout.
     */
    @Throws(IOException::class)
    override suspend fun getAvailableAddOns(): List<AddOn> {
        client.fetch(
            Request(url = "$serverURL/$API_VERSION/accounts/account/mozilla/collections/$collectionName/addons")
        ).use { response ->
            return if (response.isSuccess) {
                JSONObject(response.body.string(Charsets.UTF_8)).getAddOns()
            } else {
                emptyList()
            }
        }
    }
}

internal fun JSONObject.getAddOns(): List<AddOn> {
    val addOnsJson = getJSONArray("results")
    return (0 until addOnsJson.length()).map { index ->
        addOnsJson.getJSONObject(index).toAddOns()
    }
}

internal fun JSONObject.toAddOns(): AddOn {
    return with(getJSONObject("addon")) {
        AddOn(
            id = getSafeString("id"),
            authors = getAuthors(),
            categories = getCategories(),
            createdAt = getSafeString("created"),
            updatedAt = getSafeString("last_updated"),
            downloadUrl = getDownloadUrl(),
            version = getCurrentVersion(),
            permissions = getPermissions(),
            translatableName = getSafeMap("name"),
            translatableDescription = getSafeMap("description"),
            translatableSummary = getSafeMap("summary"),
            iconUrl = getSafeString("icon_url"),
            siteUrl = getSafeString("url"),
            rating = getRating()
        )
    }
}

internal fun JSONObject.getRating(): AddOn.Rating? {
    val jsonRating = optJSONObject("ratings")
    return if (jsonRating != null) {
        AddOn.Rating(
            reviews = jsonRating.optInt("count"),
            average = jsonRating.optDouble("average").toFloat()
        )
    } else {
        null
    }
}

internal fun JSONObject.getCategories(): List<String> {
    val jsonCategories = optJSONObject("categories")
    return if (jsonCategories == null) {
        emptyList()
    } else {
        val jsonAndroidCategories = jsonCategories.getSafeJSONArray("android")
        (0 until jsonAndroidCategories.length()).map { index ->
            jsonAndroidCategories.getString(index)
        }
    }
}

internal fun JSONObject.getPermissions(): List<String> {
    val fileJson = getJSONObject("current_version")
        .getSafeJSONArray("files")
        .getJSONObject(0)

    val permissionsJson = fileJson.getSafeJSONArray("permissions")
    return (0 until permissionsJson.length()).map { index ->
        permissionsJson.getString(index)
    }
}

internal fun JSONObject.getCurrentVersion(): String {
    return optJSONObject("current_version")?.getSafeString("version") ?: ""
}

internal fun JSONObject.getDownloadUrl(): String {
    return (getJSONObject("current_version")
        .optJSONArray("files")
        ?.getJSONObject(0))
        ?.getSafeString("url") ?: ""
}

internal fun JSONObject.getAuthors(): List<AddOn.Author> {
    val authorsJson = getSafeJSONArray("authors")
    return (0 until authorsJson.length()).map { index ->
        val authorJson = authorsJson.getJSONObject(index)

        AddOn.Author(
            id = authorJson.getSafeString("id"),
            name = authorJson.getSafeString("name"),
            username = authorJson.getSafeString("username"),
            url = authorJson.getSafeString("url")
        )
    }
}

internal fun JSONObject.getSafeString(key: String): String {
    return if (isNull(key)) {
        ""
    } else {
        getString(key)
    }
}

internal fun JSONObject.getSafeJSONArray(key: String): JSONArray {
    return if (isNull(key)) {
        JSONArray("[]")
    } else {
        getJSONArray(key)
    }
}

internal fun JSONObject.getSafeMap(valueKey: String): Map<String, String> {
    return if (isNull(valueKey)) {
        emptyMap()
    } else {
        val map = mutableMapOf<String, String>()
        val jsonObject = getJSONObject(valueKey)

        jsonObject.keys()
            .forEach { key ->
                map[key] = jsonObject.getSafeString(key)
            }
        map
    }
}
