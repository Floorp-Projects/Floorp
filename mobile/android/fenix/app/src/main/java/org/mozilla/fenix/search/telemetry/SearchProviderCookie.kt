/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.telemetry

data class SearchProviderCookie(
    val extraCodeParam: String,
    val extraCodePrefixes: List<String>,
    val host: String,
    val name: String,
    val codeParam: String,
    val codePrefixes: List<String>
)
