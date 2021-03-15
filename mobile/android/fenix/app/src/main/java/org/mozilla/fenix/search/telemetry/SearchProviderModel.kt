/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.telemetry

data class SearchProviderModel(
    val name: String,
    val regexp: Regex,
    val queryParam: String,
    val codeParam: String,
    val codePrefixes: List<String>,
    val followOnParams: List<String>,
    val extraAdServersRegexps: List<Regex>,
    val followOnCookies: List<SearchProviderCookie>
) {

    constructor(
        name: String,
        regexp: String,
        queryParam: String,
        codeParam: String = "",
        codePrefixes: List<String> = emptyList(),
        followOnParams: List<String> = emptyList(),
        extraAdServersRegexps: List<String> = emptyList(),
        followOnCookies: List<SearchProviderCookie> = emptyList()
    ) : this(
        name = name,
        regexp = regexp.toRegex(),
        queryParam = queryParam,
        codeParam = codeParam,
        codePrefixes = codePrefixes,
        followOnParams = followOnParams,
        extraAdServersRegexps = extraAdServersRegexps.map { it.toRegex() },
        followOnCookies = followOnCookies
    )

    /**
     * Checks if any of the given URLs represent an ad from the search engine.
     * Used to check if a clicked link was for an ad.
     */
    fun containsAdLinks(urlList: List<String>) = urlList.any { url -> isAd(url) }

    private fun isAd(url: String) =
        extraAdServersRegexps.any { adsRegex -> adsRegex.containsMatchIn(url) }
}
