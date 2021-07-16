/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.selectedNormalTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState

/**
 * Get the last opened normal tab and the last tab with in progress media, if available.
 *
 * @return A list of the last opened tab and the last tab with in progress media
 * if distinct and available or an empty list.
 */
fun BrowserState.asRecentTabs(): List<TabSessionState> {
    return mutableListOf<TabSessionState>().apply {
        val lastOpenedNormalTab = lastOpenedNormalTab
        lastOpenedNormalTab?.let { add(it) }
        // disabled to avoid a nightly crash in #20402
//        inProgressMediaTab
//            ?.takeUnless { it == lastOpenedNormalTab }
//            ?.let {
//                add(it)
//            }
    }
}

/**
 *  Get the selected normal tab or the last accessed normal tab
 *  if there is no selected tab or the selected tab is a private one.
 */
val BrowserState.lastOpenedNormalTab: TabSessionState?
    get() = selectedNormalTab ?: normalTabs.maxByOrNull { it.lastAccess }

/**
 * Get the last tab with in progress media.
 */
val BrowserState.inProgressMediaTab: TabSessionState?
    get() = normalTabs
        .filter { it.lastMediaAccess > 0 }
        .maxByOrNull { it.lastMediaAccess }
