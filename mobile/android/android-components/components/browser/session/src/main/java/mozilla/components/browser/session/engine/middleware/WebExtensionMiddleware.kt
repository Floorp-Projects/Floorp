/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.engine.middleware

import androidx.annotation.VisibleForTesting
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.concept.engine.EngineSession
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.support.base.log.logger.Logger

/**
 * [Middleware] implementation responsible for calling [EngineSession.markActiveForWebExtensions] on
 * [EngineSession] instances.
 */
internal class WebExtensionMiddleware : Middleware<BrowserState, BrowserAction> {
    private val logger = Logger("WebExtensionsMiddleware")
    // This is state. As such it should be in BrowserState (WebExtensionState) and not here.
    // https://github.com/mozilla-mobile/android-components/issues/7884
    @VisibleForTesting
    internal var activeWebExtensionTabId: String? = null

    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction
    ) {
        when (action) {
            is EngineAction.UnlinkEngineSessionAction -> {
                if (activeWebExtensionTabId == action.sessionId) {
                    val activeTab = context.state.findTab(action.sessionId)
                    activeTab?.engineState?.engineSession?.markActiveForWebExtensions(false)
                }
            }
        }

        next(action)

        when (action) {
            is TabListAction,
            is EngineAction.LinkEngineSessionAction -> {
                switchActiveStateIfNeeded(context.state)
            }
        }
    }

    private fun switchActiveStateIfNeeded(state: BrowserState) {
        if (activeWebExtensionTabId == state.selectedTabId) {
            return
        }

        val previousActiveTab = activeWebExtensionTabId?.let { state.findTab(it) }
        previousActiveTab?.engineState?.engineSession?.markActiveForWebExtensions(false)

        val nextActiveTab = state.selectedTabId?.let { state.findTab(it) }
        val engineSession = nextActiveTab?.engineState?.engineSession

        if (engineSession == null) {
            logger.debug("No engine session for new active tab (${nextActiveTab?.id})")
            activeWebExtensionTabId = null
            return
        } else {
            logger.debug("New active tab (${nextActiveTab.id})")
            engineSession.markActiveForWebExtensions(true)
            activeWebExtensionTabId = nextActiveTab.id
        }
    }
}
