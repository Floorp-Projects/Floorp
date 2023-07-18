/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.telemetry

import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.org.json.asSequence
import mozilla.components.support.ktx.android.org.json.toList
import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject

/**
 * Parse SERP Telemetry json from remote config.
 */

class SERPTelemetryJsonParser {
    private val remoteSettingsDownloader = RemoteSettingsDownloader()

    val searchProviders = remoteSettingsDownloader.telemetryJson().toSearchProviders()

    /**
     * Converts the json data into a list of [SearchProviderModel].
     */
    private fun JSONObject.toSearchProviders(): List<SearchProviderModel>? =
        try {
            getJSONArray("data").asSequence { i -> getJSONObject(i) }
                .mapNotNull { it.toSearchModelProvider() }
                .toList()
        } catch (e: JSONException) {
            Logger.warn("Could not parse telemetry data", e)
            null
        }

    private fun JSONObject.toSearchModelProvider(): SearchProviderModel? =
        try {
            SearchProviderModel(
                schema = getLong("schema"),
                taggedCodes = getJSONArray("taggedCodes").toList(),
                telemetryId = optString("telemetryId"),
                organicCodes = getJSONArray("organicCodes").toList(),
                searchPageRegexp = optString("searchPageRegexp"),
                queryParamName = optString("queryParamName"),
                codeParamName = optString("codeParamName"),
                followOnParamNames = optJSONArray("followOnParamNames")?.toList(),
                extraAdServersRegexps = getJSONArray("extraAdServersRegexps").toList(),
                followOnCookies = optJSONArray("followOnCookies")?.toListOfCookies(),
                expectedOrganicCodes = optJSONArray("expectedOrganicCodes")?.toList(),
            )
        } catch (e: JSONException) {
            Logger.debug("JSONException while trying to parse remote config", e)
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
                codePrefixes = optJSONArray("codePrefixes")?.toList() ?: emptyList(),
            )
        } catch (e: JSONException) {
            Logger.debug("JSONException while trying to parse remote config", e)
            null
        }
}
