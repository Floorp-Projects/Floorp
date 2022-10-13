/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.engine.middleware

import android.content.ComponentCallbacks2
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.SystemAction
import mozilla.components.browser.state.selector.allTabs
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.support.base.log.logger.Logger

// The number of tabs we keep active and do not suspend (in addition to the selected tab)
private const val MIN_ACTIVE_TABS = 3

/**
 * [Middleware] responsible for suspending [EngineSession] instances on low memory.
 */
internal class TrimMemoryMiddleware : Middleware<BrowserState, BrowserAction> {
    private val logger = Logger("TrimMemoryMiddleware")

    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction,
    ) {
        next(action)

        if (action is SystemAction.LowMemoryAction) {
            trimMemory(context, action)
        }
    }

    private fun trimMemory(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        action: SystemAction.LowMemoryAction,
    ) {
        if (!shouldCloseEngineSessions(action.level)) {
            return
        }

        val suspendTabs = determineTabsToSuspend(context.state)

        logger.info("Trim memory (tabs=${context.state.allTabs.size}, suspending=${suspendTabs.size})")

        // This is not the most efficient way of doing this. We are looping over all tabs and then
        // dispatching a SuspendEngineSessionAction for each tab that is no longer needed.
        suspendTabs.forEach { tab ->
            context.dispatch(EngineAction.SuspendEngineSessionAction(tab.id))
        }
    }

    private fun determineTabsToSuspend(
        state: BrowserState,
    ): List<SessionState> {
        return state.allTabs.filter { tab ->
            // We never suspend the currently selected tab
            tab.id != state.selectedTabId
        }.filter { tab ->
            // Only tabs with an engine session can get suspended
            tab.engineState.engineSession != null
        }.sortedByDescending { tab ->
            if (tab is TabSessionState) {
                // We want to suspend the tabs that haven't been accessed for a while first
                tab.lastAccess
            } else {
                // We are more aggressive with custom tabs an always consider them for suspension
                0
            }
        }.drop(MIN_ACTIVE_TABS) // Keep n [MIN_ACTIVE_TABS] most recently accessed tabs.
    }
}

private fun shouldCloseEngineSessions(level: Int): Boolean {
    return when (level) {
        // Foreground: The device is running extremely low on memory. The app is not yet considered a killable
        // process, but the system will begin killing background processes if apps do not release resources.
        ComponentCallbacks2.TRIM_MEMORY_RUNNING_CRITICAL -> true

        // Background: The system is running low on memory and our process is one of the first to be killed
        // if the system does not recover memory now.
        ComponentCallbacks2.TRIM_MEMORY_COMPLETE -> true

        else -> false
    }
}
