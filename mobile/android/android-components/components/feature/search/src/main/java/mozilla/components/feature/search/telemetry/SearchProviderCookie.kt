/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.telemetry

/**
 * Cookie details used to identify follow-on searches.
 *
 * @property extraCodeParamName the query parameter name in the URL that indicates
 * this might be a follow-on search.
 * @property extraCodePrefixes possible values for the query parameter in the URL that indicates
 * this might be a follow-on search.
 * @property host the hostname on which the cookie is stored.
 * @property name the name of the cookie to check.
 * @property codeParamName the name of parameter within the cookie.
 */
data class SearchProviderCookie(
    val extraCodeParamName: String,
    val extraCodePrefixes: List<String>,
    val host: String,
    val name: String,
    val codeParamName: String,
)
