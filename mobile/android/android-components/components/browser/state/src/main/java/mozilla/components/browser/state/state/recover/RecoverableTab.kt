/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state.recover

import mozilla.components.browser.state.state.ReaderState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.concept.engine.EngineSessionState

/**
 * A tab that is no longer open and in the list of tabs, but that can be restored (recovered) at
 * any time.
 *
 * The values of this data class are usually filled with the values of a [TabSessionState] when
 * getting closed.
 *
 * @param id Unique ID identifying this tba.
 * @param parentId The unique ID of the parent tab if this tab was opened from another tab (e.g. via
 * the context menu).
 * @param url The last URL of this tab.
 * @param title The last title of this tab (or an empty String).
 * @param contextId The context ID ("container") this tab used (or null).
 * @param state The [EngineSessionState] needed for restoring the previous state of this tab.
 * @param readerState The last [ReaderState] of the tab.
 * @param lastAccess The last time this tab was selected.
 */
data class RecoverableTab(
    val id: String,
    val parentId: String?,
    val url: String,
    val title: String,
    val contextId: String?,
    val state: EngineSessionState?,
    val readerState: ReaderState,
    val lastAccess: Long
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
    lastAccess = lastAccess
)
