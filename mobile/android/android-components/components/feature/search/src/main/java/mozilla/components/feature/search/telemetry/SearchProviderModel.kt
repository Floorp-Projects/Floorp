/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.telemetry

/**
 * All data needed to identify ads of a particular provider.
 *
 * @property taggedCodes array of partner codes to match against the partner code parameters in the url.
 * @property telemetryId provider name e.g. "google", "duckduckgo".
 * @property organicCodes array of partner codes to match against the parameters in the url.
 * Matching these codes will report the SERP as organic:<partner code>, which means the search
 * was performed organically rather than through a SAP.
 * @property codeParamName name of the query parameter for the partner code.
 * @property followOnCookies array of cookie details that are used to identify follow-on searches.
 * @property queryParamNames list of names of the query parameters for the user's search string.
 * @property searchPageRegexp regular expression used to match the provider.
 * @property adServerAttributes an array of strings that potentially match data-attribute keys of anchors.
 * @property followOnParamNames array of query parameter names that are used when a follow-on search occurs.
 * @property extraAdServersRegexps array of regular expressions that match URLs of potential ad servers.
 * @property expectedOrganicCodes array of partner codes to match against the parameters in the url.
 * Matching these codes will report the SERP as organic:none which means the user has done a search
 * through the search engine's website rather than through SAP.
 */
data class SearchProviderModel(
    val schema: Long,
    val taggedCodes: List<String>,
    val telemetryId: String,
    val organicCodes: List<String>?,
    val codeParamName: String,
    val followOnCookies: List<SearchProviderCookie>?,
    val queryParamNames: List<String>?,
    val searchPageRegexp: Regex,
    val adServerAttributes: List<String>?,
    val followOnParamNames: List<String>?,
    val extraAdServersRegexps: List<Regex>,
    val expectedOrganicCodes: List<String>?,

) {

    constructor(
        schema: Long,
        taggedCodes: List<String> = emptyList(),
        telemetryId: String,
        organicCodes: List<String>? = emptyList(),
        codeParamName: String = "",
        followOnCookies: List<SearchProviderCookie>? = emptyList(),
        queryParamNames: List<String> = emptyList(),
        searchPageRegexp: String,
        adServerAttributes: List<String>? = emptyList(),
        followOnParamNames: List<String>? = emptyList(),
        extraAdServersRegexps: List<String> = emptyList(),
        expectedOrganicCodes: List<String>? = emptyList(),

    ) : this(
        schema = schema,
        taggedCodes = taggedCodes,
        telemetryId = telemetryId,
        organicCodes = organicCodes,
        codeParamName = codeParamName,
        followOnCookies = followOnCookies,
        queryParamNames = queryParamNames,
        searchPageRegexp = searchPageRegexp.toRegex(),
        adServerAttributes = adServerAttributes,
        followOnParamNames = followOnParamNames,
        extraAdServersRegexps = extraAdServersRegexps.map { it.toRegex() },
        expectedOrganicCodes = expectedOrganicCodes,

    )

    /**
     * Checks if any of the given URLs represent an ad from the search engine.
     * Used to check if a clicked link was for an ad.
     */
    fun containsAdLinks(urlList: List<String>) = urlList.any { url -> isAd(url) }

    private fun isAd(url: String) =
        extraAdServersRegexps.any { adsRegex -> adsRegex.containsMatchIn(url) }
}
