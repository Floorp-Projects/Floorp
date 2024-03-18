/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.telemetry

import mozilla.appservices.remotesettings.RemoteSettingsResponse
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.org.json.asSequence
import mozilla.components.support.ktx.android.org.json.toList
import mozilla.components.support.remotesettings.RemoteSettingsClient
import mozilla.components.support.remotesettings.RemoteSettingsResult
import org.jetbrains.annotations.VisibleForTesting
import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject
import java.io.File

internal const val REMOTE_PROD_ENDPOINT_URL = "https://firefox.settings.services.mozilla.com"
internal const val REMOTE_ENDPOINT_BUCKET_NAME = "main"

/**
 * Parse SERP Telemetry json from remote config.
 */
class SerpTelemetryRepository(
    rootStorageDirectory: File,
    private val readJson: () -> JSONObject,
    collectionName: String,
    serverUrl: String = REMOTE_PROD_ENDPOINT_URL,
    bucketName: String = REMOTE_ENDPOINT_BUCKET_NAME,
) {
    val logger = Logger("SerpTelemetryRepository")
    private var providerList: List<SearchProviderModel> = emptyList()

    @VisibleForTesting
    internal var remoteSettingsClient = RemoteSettingsClient(
        serverUrl = serverUrl,
        bucketName = bucketName,
        collectionName = collectionName,
        storageRootDirectory = rootStorageDirectory,
    )

    /**
     * Provides list of search providers from cache or dump and fetches from remotes server .
     */
    suspend fun updateProviderList(): List<SearchProviderModel> {
        val (cacheLastModified, cacheResponse) = loadProvidersFromCache()
        val localResponse = readJson()
        if (cacheResponse.isEmpty() || cacheLastModified <= localResponse.getString("timestamp").toULong()) {
            providerList = parseLocalPreinstalledData(localResponse)
        } else if (cacheLastModified > localResponse.getString("timestamp").toULong()) {
            providerList = cacheResponse
        }
        fetchRemoteResponse(cacheLastModified)
        return providerList
    }

    @VisibleForTesting
    internal suspend fun fetchRemoteResponse(
        cacheLastModified: ULong?,
    ) {
        if (cacheLastModified == null) {
            return
        }
        val remoteResponse = fetchRemoteResponse()
        if (remoteResponse.lastModified > cacheLastModified) {
            providerList = parseRemoteResponse(remoteResponse)
            writeToCache(remoteResponse)
        }
    }

    /**
     * Writes data to local cache.
     */
    @VisibleForTesting
    internal suspend fun writeToCache(records: RemoteSettingsResponse): RemoteSettingsResult {
        return remoteSettingsClient.write(records)
    }

    /**
     * Parses local json response.
     */
    @VisibleForTesting
    internal fun parseLocalPreinstalledData(jsonObject: JSONObject): List<SearchProviderModel> {
        return jsonObject.getJSONArray("data")
            .asSequence()
            .mapNotNull {
                (it as JSONObject).toSearchProviderModel()
            }
            .toList()
    }

    /**
     * Parses remote server response.
     */
    private fun parseRemoteResponse(response: RemoteSettingsResponse): List<SearchProviderModel> {
        return response.records.mapNotNull {
            it.fields.toSearchProviderModel()
        }
    }

    /**
     * Returns data from remote servers.
     */
    @VisibleForTesting
    internal suspend fun fetchRemoteResponse(): RemoteSettingsResponse {
        val result = remoteSettingsClient.fetch()
        return if (result is RemoteSettingsResult.Success) {
            result.response
        } else {
            RemoteSettingsResponse(emptyList(), 0u)
        }
    }

    /**
     * Returns search providers from local cache.
     */
    @VisibleForTesting
    internal suspend fun loadProvidersFromCache(): Pair<ULong, List<SearchProviderModel>> {
        val result = remoteSettingsClient.read()
        return if (result is RemoteSettingsResult.Success) {
            val response = result.response.records.mapNotNull {
                it.fields.toSearchProviderModel()
            }
            val lastModified = result.response.lastModified
            Pair(lastModified, response)
        } else {
            Pair(0u, emptyList())
        }
    }
}

@VisibleForTesting
internal fun JSONObject.toSearchProviderModel(): SearchProviderModel? =
    try {
        SearchProviderModel(
            schema = getLong("schema"),
            taggedCodes = getJSONArray("taggedCodes").toList(),
            telemetryId = optString("telemetryId"),
            organicCodes = getJSONArray("organicCodes").toList(),
            codeParamName = optString("codeParamName"),
            followOnCookies = optJSONArray("followOnCookies")?.toListOfCookies(),
            queryParamNames = optJSONArray("queryParamNames").toList(),
            searchPageRegexp = optString("searchPageRegexp"),
            adServerAttributes = optJSONArray("adServerAttributes").toList(),
            followOnParamNames = optJSONArray("followOnParamNames")?.toList(),
            extraAdServersRegexps = getJSONArray("extraAdServersRegexps").toList(),
            expectedOrganicCodes = optJSONArray("expectedOrganicCodes")?.toList(),
        )
    } catch (e: JSONException) {
        Logger("SerpTelemetryRepository").error("JSONException while trying to parse remote config", e)
        null
    }

private fun JSONArray.toListOfCookies(): List<SearchProviderCookie> =
    toList<JSONObject>().mapNotNull { jsonObject -> jsonObject.toSearchProviderCookie() }

private fun JSONObject.toSearchProviderCookie(): SearchProviderCookie? =
    try {
        SearchProviderCookie(
            extraCodeParamName = optString("extraCodeParamName"),
            extraCodePrefixes = getJSONArray("extraCodePrefixes").toList(),
            host = optString("host"),
            name = optString("name"),
            codeParamName = optString("codeParamName"),
        )
    } catch (e: JSONException) {
        Logger("SerpTelemetryRepository").error("JSONException while trying to parse remote config", e)
        null
    }
