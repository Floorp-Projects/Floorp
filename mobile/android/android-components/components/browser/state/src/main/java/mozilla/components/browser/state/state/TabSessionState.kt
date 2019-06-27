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
 */
data class TabSessionState(
    override val id: String = UUID.randomUUID().toString(),
    override val content: ContentState
) : SessionState

internal fun createTab(
    url: String,
    private: Boolean = false,
    id: String = UUID.randomUUID().toString()
): TabSessionState {
    return TabSessionState(
        id = id,
        content = ContentState(url, private)
    )
}

internal fun createCustomTab(url: String, id: String = UUID.randomUUID().toString()): CustomTabSessionState {
    return CustomTabSessionState(
        id = id,
        content = ContentState(url)
    )
}
