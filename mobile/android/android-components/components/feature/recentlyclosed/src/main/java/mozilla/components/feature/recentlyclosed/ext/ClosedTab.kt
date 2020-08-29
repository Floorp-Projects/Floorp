/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.recentlyclosed.ext

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.action.RecentlyClosedAction
import mozilla.components.browser.state.state.ClosedTab
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.store.BrowserStore

/**
 * Restores the given [ClosedTab]. On restore, will invoke [onTabRestored] and remove the
 * closed tab from the [BrowserState].
 */
fun ClosedTab.restoreTab(
    browserStore: BrowserStore,
    sessionManager: SessionManager,
    onTabRestored: () -> Unit
) {
    val session = Session(this.url, false, SessionState.Source.RESTORED)
    sessionManager.add(
        session,
        selected = true,
        engineSessionState = this.engineSessionState
    )
    browserStore.dispatch(RecentlyClosedAction.RemoveClosedTabAction(this))
    onTabRestored()
}
