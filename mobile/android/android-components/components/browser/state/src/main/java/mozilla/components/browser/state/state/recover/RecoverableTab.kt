/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state.recover

import mozilla.components.browser.state.state.ReaderState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.concept.storage.HistoryMetadataKey

/**
 * A tab that is no longer open and in the list of tabs, but that can be restored (recovered) at
 * any time.
 *
 * The values of this data class are usually filled with the values of a [TabSessionState] when
 * getting closed.
 *
 * @property id Unique ID identifying this tab.
 * @property url The last URL of this tab.
 * @property parentId The unique ID of the parent tab if this tab was opened from another tab (e.g. via
 * the context menu).
 * @property title The last title of this tab (or an empty String).
 * @property contextId The context ID ("container") this tab used (or null).
 * @property state The [EngineSessionState] needed for restoring the previous state of this tab.
 * @property readerState The last [ReaderState] of the tab.
 * @property lastAccess The last time this tab was selected.
 * @property private If tab was private.
 */
data class RecoverableTab(
    val id: String,
    val url: String,
    val parentId: String? = null,
    val title: String = "",
    val contextId: String? = null,
    val state: EngineSessionState? = null,
    val readerState: ReaderState = ReaderState(),
    val lastAccess: Long = 0,
    val private: Boolean = false,
    val historyMetadata: HistoryMetadataKey? = null
)

/**
 * Creates a [RecoverableTab] from this [TabSessionState].
 */
fun TabSessionState.toRecoverableTab() = RecoverableTab(
    id = id,
    parentId = parentId,
    url = content.url,
    title = content.title,
    contextId = contextId,
    state = engineState.engineSessionState,
    readerState = readerState,
    lastAccess = lastAccess,
    private = content.private,
    historyMetadata = historyMetadata
)

/**
 * Creates a list of [RecoverableTab]s from a List of [TabSessionState]s.
 */
fun List<TabSessionState>.toRecoverableTabs() = map { it.toRecoverableTab() }
