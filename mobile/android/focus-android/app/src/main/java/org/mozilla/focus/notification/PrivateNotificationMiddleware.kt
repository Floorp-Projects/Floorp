/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.notification

import android.content.Context
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.action.UndoAction
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.focus.session.SessionNotificationService

/**
 * Middleware responsible for launching the [SessionNotificationService] and showing an ongoing
 * notification while private tabs are open.
 */
class PrivateNotificationMiddleware(
    private val applicationContext: Context
) : Middleware<BrowserState, BrowserAction> {
    private var isServiceRunning: Boolean = false

    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction
    ) {
        next(action)

        if (action is TabListAction || action is UndoAction) {
            val hasPrivateTabs = context.state.privateTabs.isNotEmpty()
            if (isServiceRunning != hasPrivateTabs) {
                startOrStopService(hasPrivateTabs)
            }
        }
    }

    private fun startOrStopService(hasPrivateTabs: Boolean) {
        isServiceRunning = if (hasPrivateTabs) {
            SessionNotificationService.start(applicationContext)
            true
        } else {
            SessionNotificationService.stop(applicationContext)
            false
        }
    }
}
