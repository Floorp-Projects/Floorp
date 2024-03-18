/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import mozilla.components.browser.state.search.SearchEngine
import org.mozilla.fenix.components.Core.Companion.BOOKMARKS_SEARCH_ENGINE_ID
import org.mozilla.fenix.components.Core.Companion.HISTORY_SEARCH_ENGINE_ID
import org.mozilla.fenix.components.Core.Companion.TABS_SEARCH_ENGINE_ID

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
    "startpage",
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

/**
 * Return safe search engine name for telemetry purposes.
 */
fun SearchEngine.telemetryName(): String =
    when (type) {
        SearchEngine.Type.CUSTOM -> "custom"
        SearchEngine.Type.APPLICATION ->
            when (id) {
                HISTORY_SEARCH_ENGINE_ID -> "history"
                BOOKMARKS_SEARCH_ENGINE_ID -> "bookmarks"
                TABS_SEARCH_ENGINE_ID -> "tabs"
                else -> "application"
            }
        else -> name
    }
