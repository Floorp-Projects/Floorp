/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.action

import mozilla.components.browser.state.selector.findTabOrCustomTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.lib.state.Store

/**
 * Interface for [BrowserAction]s that reference a tab ([SessionState]) via the provided [tabId].
 */
interface ActionWithTab {
    val tabId: String
}

/**
 * Looks up the tab referenced by this [ActionWithTab] in the provided [Store] and returns it.
 * Returns `null` if the tab could not be found.
 */
fun ActionWithTab.lookupTabIn(store: Store<BrowserState, BrowserAction>): SessionState? {
    return store.state.findTabOrCustomTab(tabId)
}

/**
 * Casts this [ActionWithTab] to a [BrowserAction].
 */
fun ActionWithTab.toBrowserAction(): BrowserAction {
    return this as BrowserAction
}
