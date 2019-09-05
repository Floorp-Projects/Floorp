/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

import java.util.UUID

/**
 * Value type that represents the state of a tab (private or normal).
 *
 * @property id the ID of this tab and session.
 * @property content the [ContentState] of this tab.
 * @property trackingProtection the [TrackingProtectionState] of this tab.
 * @property parentId the parent ID of this tab or null if this tab has no
 * parent. The parent tab is usually the tab that initiated opening this
 * tab (e.g. the user clicked a link with target="_blank" or selected
 * "open in new tab" or a "window.open" was triggered).
 */
data class TabSessionState(
    override val id: String = UUID.randomUUID().toString(),
    override val content: ContentState,
    override val trackingProtection: TrackingProtectionState = TrackingProtectionState(),
    override val engineState: EngineState = EngineState(),
    val parentId: String? = null
) : SessionState

fun createTab(
    url: String,
    private: Boolean = false,
    id: String = UUID.randomUUID().toString(),
    parent: TabSessionState? = null
): TabSessionState {
    return TabSessionState(
        id = id,
        content = ContentState(url, private),
        parentId = parent?.id
    )
}
