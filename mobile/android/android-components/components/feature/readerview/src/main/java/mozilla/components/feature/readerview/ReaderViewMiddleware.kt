/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.readerview

import androidx.annotation.VisibleForTesting
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.ReaderAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.feature.readerview.ReaderViewFeature.Companion.READER_VIEW_EXTENSION_ID
import mozilla.components.feature.readerview.ReaderViewFeature.Companion.READER_VIEW_EXTENSION_URL
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareStore
import mozilla.components.support.webextensions.WebExtensionController

/**
 * [Middleware] implementation for translating [BrowserAction]s to
 * [ReaderAction]s (e.g. if the URL is updated a new "readerable"
 * check should be executed.)
 */
class ReaderViewMiddleware : Middleware<BrowserState, BrowserAction> {

    @VisibleForTesting
    internal var extensionController = WebExtensionController(READER_VIEW_EXTENSION_ID, READER_VIEW_EXTENSION_URL)

    override fun invoke(
        store: MiddlewareStore<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction
    ) {
        preProcess(store, action)
        next(action)
        postProcess(store, action)
    }

    private fun preProcess(
        store: MiddlewareStore<BrowserState, BrowserAction>,
        action: BrowserAction
    ) {
        when (action) {
            // We want to bind the feature instance to the lifecycle of the browser
            // fragment so it won't necessarily be active when a tab is removed
            // (e.g. via a tabs tray fragment). In order to disconnect the port as
            // early as possible it's best to do it here directly.
            is EngineAction.UnlinkEngineSessionAction -> {
                store.state.findTab(action.sessionId)?.engineState?.engineSession?.let {
                    extensionController.disconnectPort(it, READER_VIEW_EXTENSION_ID)
                }
            }
        }
    }

    private fun postProcess(
        store: MiddlewareStore<BrowserState, BrowserAction>,
        action: BrowserAction
    ) {
        when (action) {
            is TabListAction.SelectTabAction -> {
                store.dispatch(ReaderAction.UpdateReaderableAction(action.tabId, false))
                store.dispatch(ReaderAction.UpdateReaderableCheckRequiredAction(action.tabId, true))
            }
            is ContentAction.UpdateUrlAction -> {
                store.dispatch(ReaderAction.UpdateReaderableAction(action.sessionId, false))
                store.dispatch(ReaderAction.UpdateReaderableCheckRequiredAction(action.sessionId, true))
            }
            is EngineAction.LinkEngineSessionAction -> {
                store.dispatch(ReaderAction.UpdateReaderConnectRequiredAction(action.sessionId, true))
            }
        }
    }
}
