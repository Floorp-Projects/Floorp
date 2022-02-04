/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.cfr

import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.focus.Components
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.utils.ERASE_CFR_LIMIT

/**
 * Middleware used to intercept browser store actions in order to decide when should we display a specific CFR
 */
class CfrMiddleware(private val components: Components) : Middleware<BrowserState, BrowserAction> {
    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction
    ) {
        next(action)
        if (action is TabListAction.AddTabAction) {
            components.settings.numberOfTabsOpened++
            if (components.settings.numberOfTabsOpened == ERASE_CFR_LIMIT) {
                components.appStore.dispatch(AppAction.ShowEraseTabsCfrChange(true))
            }
        }
    }
}
