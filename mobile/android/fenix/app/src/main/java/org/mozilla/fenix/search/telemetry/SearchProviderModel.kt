/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.telemetry

data class SearchProviderModel(
    val name: String,
    val regexp: String,
    val queryParam: String,
    val codeParam: String = "",
    val codePrefixes: List<String> = ArrayList(),
    val followOnParams: List<String> = ArrayList(),
    val extraAdServersRegexps: List<String> = ArrayList(),
    val followOnCookies: List<SearchProviderCookie> = ArrayList()
)
