/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import mozilla.components.browser.state.search.SearchEngine

// List of well known search domains, taken from
// https://searchfox.org/mozilla-central/source/toolkit/components/search/SearchService.jsm#2405
private val wellKnownSearchDomains = setOf(
    "aol",
    "ask",
    "baidu",
    "bing",
    "duckduckgo",
    "google",
    "yahoo",
    "yandex",
    "startpage"
)

/**
 * Whether or not the search engine is a custom engine added by the user.
 */
fun SearchEngine.isCustomEngine(): Boolean =
    this.type == SearchEngine.Type.CUSTOM

/**
 * Whether or not the search engine is a known search domain.
 */
fun SearchEngine.isKnownSearchDomain(): Boolean =
    this.resultUrls[0].findAnyOf(wellKnownSearchDomains, 0, true) != null
