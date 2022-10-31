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
private val validCodeSet = setOf(
    "MOZ2", "MOZ4", "MOZ5", "MOZA", "MOZB", "MOZD", "MOZE", "MOZI", "MOZM", "MOZO", "MOZT",
    "MOZW", "MOZSL01", "MOZSL02", "MOZSL03", "monline_dg", "monline_3_dg", "monline_4_dg",
    "monline_7_dg", "firefox-a", "firefox-b", "firefox-b-1", "firefox-b-ab", "firefox-b-1-ab",
    "firefox-b-d", "firefox-b-1-d", "firefox-b-e", "firefox-b-1-e", "firefox-b-m",
    "firefox-b-1-m", "firefox-b-o", "firefox-b-1-o", "firefox-b-lm", "firefox-b-1-lm",
    "firefox-b-lg", "firefox-b-huawei-h1611", "firefox-b-is-oem1", "firefox-b-oem1",
    "firefox-b-oem2", "firefox-b-tinno", "firefox-b-pn-wt", "firefox-b-pn-wt-us", "ubuntu",
    "ffab", "ffcm", "ffhp", "ffip", "ffit", "ffnt", "ffocus", "ffos", "ffsb", "fpas", "fpsa",
    "ftas", "ftsa", "newext", "1000969a", null,
)
private val validChannelSet = setOf("ts")

/**
 * Get a String in a specific format allowing to identify how an ads/search provider was used.
 *
 * @see [TrackKeyInfo.createTrackKey]
 */
internal fun getTrackKey(
    provider: SearchProviderModel,
    uri: Uri,
    cookies: List<JSONObject>,
): String {
    val paramSet = uri.queryParameterNames
    var code: String? = null

    if (provider.codeParam.isNotEmpty()) {
        code = uri.getQueryParameter(provider.codeParam)
        if (code.isNullOrEmpty() &&
            provider.name == "baidu" &&
            uri.toString().contains("from=")
        ) {
            code = uri.toString().substringAfter("from=", "")
                .substringBefore("/", "")
        }

        // For Bug 1751920
        if (!validCodeSet.contains(code)) {
            code = "other"
        }

        // Glean doesn't allow code starting with a figure
        if (code != null && code.isNotEmpty()) {
            val codeStart = code.first()
            if (codeStart.isDigit()) {
                code = "_$code"
            }
        }

        // Try cookies first because Bing has followOnCookies and valid code, but no
        // followOnParams => would tracks organic instead of sap-follow-on
        if (provider.followOnCookies.isNotEmpty()) {
            // Checks if engine contains a valid follow-on cookie, otherwise return default
            getTrackKeyFromCookies(provider, uri, cookies)?.let {
                return it.createTrackKey()
            }
        }

        // For Bing if it didn't have a valid cookie and for all the other search engines
        if (hasValidCode(uri.getQueryParameter(provider.codeParam), provider)) {
            var channel = uri.getQueryParameter(CHANNEL_KEY)

            // For Bug 1751955
            if (!validChannelSet.contains(channel)) {
                channel = null
            }

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
    cookies: List<JSONObject>,
): TrackKeyInfo? {
    // Especially Bing requires lots of extra work related to cookies.
    for (followOnCookie in provider.followOnCookies) {
        val eCode = uri.getQueryParameter(followOnCookie.extraCodeParam)
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

            if (valueList.size == 2 && valueList[0] == followOnCookie.codeParam &&
                followOnCookie.codePrefixes.any { prefix ->
                    valueList[1].startsWith(
                            prefix,
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
