/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.engine.middleware

import android.content.ComponentCallbacks2
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.SystemAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext

/**
 * [Middleware] responsible for suspending [EngineSession] instances on low memory.
 */
internal class TrimMemoryMiddleware : Middleware<BrowserState, BrowserAction> {
    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction
    ) {
        next(action)

        if (action is SystemAction.LowMemoryAction) {
            trimMemory(context, action)
        }
    }

    private fun trimMemory(
        store: MiddlewareContext<BrowserState, BrowserAction>,
        action: SystemAction.LowMemoryAction
    ) {
        if (!shouldCloseEngineSessions(action.level)) {
            return
        }

        // This is not the most efficient way of doing this. We are looping over all tabs and then
        // dispatching a SuspendEngineSessionAction for each tab that is no longer needed.
        (store.state.tabs + store.state.customTabs).forEach { tab ->
            if (tab.id != store.state.selectedTabId) {
                store.dispatch(
                    EngineAction.SuspendEngineSessionAction(tab.id)
                )
            }
        }
    }
}

private fun shouldCloseEngineSessions(level: Int): Boolean {
    return when (level) {
        // Foreground: The device is running extremely low on memory. The app is not yet considered a killable
        // process, but the system will begin killing background processes if apps do not release resources.
        ComponentCallbacks2.TRIM_MEMORY_RUNNING_CRITICAL -> true

        // Background: The system is running low on memory and our process is near the middle of the LRU list.
        // If the system becomes further constrained for memory, there's a chance our process will be killed.
        ComponentCallbacks2.TRIM_MEMORY_MODERATE,
            // Background: The system is running low on memory and our process is one of the first to be killed
            // if the system does not recover memory now.
        ComponentCallbacks2.TRIM_MEMORY_COMPLETE -> true

        else -> false
    }
}
