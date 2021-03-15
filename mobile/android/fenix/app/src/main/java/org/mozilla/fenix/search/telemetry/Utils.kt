/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.telemetry

import android.net.Uri
import org.json.JSONObject

private const val SEARCH_TYPE_SAP_FOLLOW_ON = "sap-follow-on"
private const val SEARCH_TYPE_SAP = "sap"
private const val SEARCH_TYPE_ORGANIC = "organic"
private const val CHANNEL_KEY = "channel"

internal fun getTrackKey(
    provider: SearchProviderModel,
    uri: Uri,
    cookies: List<JSONObject>
): String {
    val paramSet = uri.queryParameterNames
    var code: String? = null

    if (provider.codeParam.isNotEmpty()) {
        code = uri.getQueryParameter(provider.codeParam)

        // Try cookies first because Bing has followOnCookies and valid code, but no
        // followOnParams => would tracks organic instead of sap-follow-on
        if (provider.followOnCookies.isNotEmpty()) {
            // Checks if engine contains a valid follow-on cookie, otherwise return default
            getTrackKeyFromCookies(provider, uri, cookies)?.let {
                return it.createTrackKey()
            }
        }

        // For Bing if it didn't have a valid cookie and for all the other search engines
        if (hasValidCode(code, provider)) {
            val channel = uri.getQueryParameter(CHANNEL_KEY)
            val type = getSapType(provider.followOnParams, paramSet)
            return TrackKeyInfo(provider.name, type, code, channel).createTrackKey()
        }
    }

    // Default to organic search type if no code parameter was found.
    return TrackKeyInfo(provider.name, SEARCH_TYPE_ORGANIC, code).createTrackKey()
}

private fun getTrackKeyFromCookies(
    provider: SearchProviderModel,
    uri: Uri,
    cookies: List<JSONObject>
): TrackKeyInfo? {
    // Especially Bing requires lots of extra work related to cookies.
    for (followOnCookie in provider.followOnCookies) {
        val eCode = uri.getQueryParameter(followOnCookie.extraCodeParam)
        if (eCode == null || !followOnCookie.extraCodePrefixes.any { prefix ->
                eCode.startsWith(prefix)
            }) {
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

            if (valueList.size == 2 && valueList[0] == followOnCookie.codeParam &&
                followOnCookie.codePrefixes.any { prefix ->
                    valueList[1].startsWith(
                        prefix
                    )
                }
            ) {
                return TrackKeyInfo(provider.name, SEARCH_TYPE_SAP_FOLLOW_ON, valueList[1])
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
    code != null && provider.codePrefixes.any { prefix -> code.startsWith(prefix) }
