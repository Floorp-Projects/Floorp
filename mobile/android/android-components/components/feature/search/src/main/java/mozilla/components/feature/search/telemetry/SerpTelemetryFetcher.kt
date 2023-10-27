/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.telemetry

import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.org.json.toList
import mozilla.components.support.remotesettings.RemoteSettingsClient
import mozilla.components.support.remotesettings.RemoteSettingsResult
import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject
import java.io.File

internal const val REMOTE_ENDPOINT_URL = "https://firefox.settings.services.mozilla.com"
internal const val REMOTE_ENDPOINT_BUCKET_NAME = "main"

/**
 * Parse SERP Telemetry json from remote config.
 */
class SerpTelemetryFetcher(
    rootStorageDirectory: File,
    collectionName: String,
    serverUrl: String = REMOTE_ENDPOINT_URL,
    bucketName: String = REMOTE_ENDPOINT_BUCKET_NAME,
) {
    val logger = Logger("SerpTelemetryFetcher")
    private val remoteSettingsClient = RemoteSettingsClient(
        serverUrl = serverUrl,
        bucketName = bucketName,
        collectionName = collectionName,
        storageRootDirectory = rootStorageDirectory,
    )

    /**
     * Returns search providers.
     */
    suspend fun fetchSearchProviders(): List<SearchProviderModel> =
        when (val result = remoteSettingsClient.fetch()) {
            is RemoteSettingsResult.Success -> {
                result.response.records.mapNotNull { record ->
                    record.fields.toSearchModelProvider()
                }.also { searchProviderModels ->
                    // Update local cache if we successfully fetched new results
                    if (searchProviderModels.isNotEmpty()) {
                        remoteSettingsClient.write(result.response)
                    }
                }
            }

            else -> emptyList()
        }

    private fun JSONObject.toSearchModelProvider(): SearchProviderModel? =
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
            logger.error("JSONException while trying to parse remote config", e)
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
            logger.error("JSONException while trying to parse remote config", e)
            null
        }
}
