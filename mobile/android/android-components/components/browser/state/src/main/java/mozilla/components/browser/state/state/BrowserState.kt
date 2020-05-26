/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.lib.state.State

/**
 * Value type that represents the complete state of the browser/engine.
 *
 * @property tabs the list of open tabs, defaults to an empty list.
 * @property selectedTabId the ID of the currently selected (active) tab.
 * @property customTabs the list of custom tabs, defaults to an empty list.
 * @property extensions A map of extension ids and web extensions of all installed web extensions.
 * The extensions here represent the default values for all [BrowserState.extensions] and can
 * be overridden per [SessionState].
 * @property media The state of all media elements and playback states for all tabs.
 * @property queuedDownloads queued downloads ([DownloadState]s) mapped to their IDs.
 */
data class BrowserState(
    val tabs: List<TabSessionState> = emptyList(),
    val selectedTabId: String? = null,
    val customTabs: List<CustomTabSessionState> = emptyList(),
    val extensions: Map<String, WebExtensionState> = emptyMap(),
    val media: MediaState = MediaState(),
    val queuedDownloads: Map<Long, DownloadState> = emptyMap()
) : State
