/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.telemetry

import android.net.Uri
import org.json.JSONObject

private const val SEARCH_TYPE_SAP_FOLLOW_ON = "sap-follow-on"
private const val SEARCH_TYPE_SAP = "sap"
private const val SEARCH_TYPE_ORGANIC = "organic"
private const val CHANNEL_KEY = "channel"
private val validChannelSet = setOf("ts")

/**
 * Get a String in a specific format allowing to identify how an ads/search provider was used.
 *
 * @see [TrackKeyInfo.createTrackKey]
 */
@Suppress("NestedBlockDepth", "ComplexMethod")
internal fun getTrackKey(
    provider: SearchProviderModel,
    uri: Uri,
    cookies: List<JSONObject>,
): String {
    var type = SEARCH_TYPE_ORGANIC
    val paramSet = uri.queryParameterNames
    var code: String? = null

    if (provider.codeParamName.isNotEmpty()) {
        code = uri.getQueryParameter(provider.codeParamName)
        if (code.isNullOrEmpty() &&
            provider.telemetryId == "baidu" &&
            uri.toString().contains("from=")
        ) {
            code = uri.toString().substringAfter("from=", "")
                .substringBefore("/", "")
        }
        if (!code.isNullOrEmpty()) {
            if (provider.taggedCodes.contains(code)) {
                type = SEARCH_TYPE_SAP
                if (!provider.followOnParamNames.isNullOrEmpty() &&
                    provider.followOnParamNames.any { prefix ->
                        paramSet.contains(prefix)
                    }
                ) {
                    type = SEARCH_TYPE_SAP_FOLLOW_ON
                }
            } else if (provider.organicCodes?.contains(code) == true) {
                type = SEARCH_TYPE_ORGANIC
            } else if (provider.expectedOrganicCodes?.contains(code) == true) {
                code = "none"
            } else {
                code = "other"
            }

            // Glean doesn't allow code starting with a figure
            val codeStart = code.first()
            if (codeStart.isDigit()) {
                code = "_$code"
            }
        }

        // Try cookies first because Bing has followOnCookies and valid code, but no
        // followOnParams => would tracks organic instead of sap-follow-on
        if (!provider.followOnCookies.isNullOrEmpty()) {
            // Checks if engine contains a valid follow-on cookie, otherwise return default
            getTrackKeyFromCookies(provider, uri, cookies)?.let {
                return it.createTrackKey()
            }
        }

        // For Bing if it didn't have a valid cookie and for all the other search engines
        if (hasValidCode(uri.getQueryParameter(provider.codeParamName), provider)) {
            var channel = uri.getQueryParameter(CHANNEL_KEY)

            // For Bug 1751955
            if (!validChannelSet.contains(channel)) {
                channel = null
            }

            type = if (provider.followOnParamNames == null) {
                SEARCH_TYPE_SAP
            } else {
                getSapType(provider.followOnParamNames, paramSet)
            }
            return TrackKeyInfo(provider.telemetryId, type, code, channel).createTrackKey()
        }
    }

    // Default to organic search type if no code parameter was found.
    if (code.isNullOrEmpty()) {
        code = "none"
    }
    return TrackKeyInfo(provider.telemetryId, type, code).createTrackKey()
}

private fun getTrackKeyFromCookies(
    provider: SearchProviderModel,
    uri: Uri,
    cookies: List<JSONObject>,
): TrackKeyInfo? {
    // Especially Bing requires lots of extra work related to cookies.
    for (followOnCookie in provider.followOnCookies!!) {
        val eCode = uri.getQueryParameter(followOnCookie.extraCodeParamName)
        if (eCode == null || !followOnCookie.extraCodePrefixes.any { prefix ->
                eCode.startsWith(prefix)
            }
        ) {
            continue
        }

        // If this cookie is present, it's probably an SAP follow-on.
        // This might be an organic follow-on in the same session, but there
        // is no way to tell the difference.
        for (cookie in cookies) {
            if (cookie.getString("name") != followOnCookie.name) {
                continue
            }
            val valueList = cookie.getString("value")
                .split("=")
                .map { item -> item.trim() }

            if (valueList.size == 2 && valueList[0] == followOnCookie.codeParamName &&
                provider.taggedCodes.any { prefix ->
                    valueList[1].startsWith(
                        prefix,
                    )
                }
            ) {
                return TrackKeyInfo(provider.telemetryId, SEARCH_TYPE_SAP_FOLLOW_ON, valueList[1])
            }
        }
    }

    return null
}

private fun getSapType(followOnParams: List<String>, paramSet: Set<String>): String {
    return if (followOnParams.any { param -> paramSet.contains(param) }) {
        SEARCH_TYPE_SAP_FOLLOW_ON
    } else {
        SEARCH_TYPE_SAP
    }
}

private fun hasValidCode(code: String?, provider: SearchProviderModel): Boolean =
    code != null && provider.taggedCodes.any { prefix -> code.startsWith(prefix) }
