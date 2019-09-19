/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.selector

import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.TabSessionState

// Extension functions for querying and dissecting [BrowserState]

/**
 * The currently selected tab if there's one.
 *
 * Only one [TabSessionState] can be selected at a given time.
 */
val BrowserState.selectedTab: TabSessionState?
    get() = selectedTabId?.let { id -> findTab(id) }

/**
 * Finds and returns the tab with the given id. Returns null if no matching tab could be
 * found.
 *
 * @param tabId The ID of the tab to search for.
 * @return The [TabSessionState] with the provided [tabId] or null if it could not be found.
 */
fun BrowserState.findTab(tabId: String): TabSessionState? {
    return tabs.firstOrNull { it.id == tabId }
}

/**
 * Finds and returns the Custom Tab with the given id. Returns null if no matching tab could be
 * found.
 *
 * @param tabId The ID of the custom tab to search for.
 * @return The [CustomTabSessionState] with the provided [tabId] or null if it could not be found.
 */
fun BrowserState.findCustomTab(tabId: String): CustomTabSessionState? {
    return customTabs.firstOrNull { it.id == tabId }
}

/**
 * Finds and returns the [TabSessionState] or [CustomTabSessionState] with the given [tabId].
 */
fun BrowserState.findTabOrCustomTab(tabId: String): SessionState? {
    return findTab(tabId) ?: findCustomTab(tabId)
}

/**
 * Finds and returns the tab with the given id or the selected tab if no id was provided (null). Returns null
 * if no matching tab could be found or if no selected tab exists.
 *
 * @param customTabId An optional ID of a custom tab. If not provided or null then the selected tab will be returned.
 * @return The custom tab with the provided ID or the selected tav if no id was provided.
 */
fun BrowserState.findCustomTabOrSelectedTab(customTabId: String? = null): SessionState? {
    return if (customTabId != null) {
        findCustomTab(customTabId)
    } else {
        selectedTab
    }
}

/**
 * List of private tabs.
 */
val BrowserState.privateTabs: List<TabSessionState>
    get() = tabs.filter { it.content.private }

/**
 * List of normal (non-private) tabs.
 */
val BrowserState.normalTabs: List<TabSessionState>
    get() = tabs.filter { !it.content.private }
