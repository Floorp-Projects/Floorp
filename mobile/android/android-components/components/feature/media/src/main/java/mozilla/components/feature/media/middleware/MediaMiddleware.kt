/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.middleware

import android.content.Context
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PRIVATE
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.CustomTabListAction
import mozilla.components.browser.state.action.MediaAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.MediaState
import mozilla.components.feature.media.middleware.sideeffects.MediaAggregateUpdater
import mozilla.components.feature.media.middleware.sideeffects.MediaFactsEmitter
import mozilla.components.feature.media.middleware.sideeffects.MediaServiceLauncher
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext

/**
 * [Middleware] implementation for updating the [MediaState.Aggregate] and launching an
 * [MediaServiceLauncher]
 */
class MediaMiddleware(
    context: Context,
    mediaServiceClass: Class<*>
) : Middleware<BrowserState, BrowserAction> {
    @VisibleForTesting(otherwise = PRIVATE)
    internal val mediaAggregateUpdate = MediaAggregateUpdater()
    private val mediaServiceLauncher = MediaServiceLauncher(context, mediaServiceClass)
    private val mediaFactsEmitter = MediaFactsEmitter()

    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction
    ) {
        preProcess(context, action)
        next(action)
        postProcess(context, action)
    }

    private fun preProcess(
        store: MiddlewareContext<BrowserState, BrowserAction>,
        action: BrowserAction
    ) {
        when (action) {
            is TabListAction.RemoveAllTabsAction ->
                store.dispatch(MediaAction.RemoveTabMediaAction(
                    store.state.tabs.map { it.id }
                ))

            is TabListAction.RemoveAllNormalTabsAction ->
                store.dispatch(MediaAction.RemoveTabMediaAction(
                    store.state.normalTabs.map { it.id }
                ))

            is TabListAction.RemoveAllPrivateTabsAction ->
                store.dispatch(MediaAction.RemoveTabMediaAction(
                    store.state.privateTabs.map { it.id }
                ))

            is TabListAction.RemoveTabAction ->
                store.dispatch(MediaAction.RemoveTabMediaAction(
                    listOf(action.tabId)
                ))

            is CustomTabListAction.RemoveAllCustomTabsAction ->
                store.dispatch(MediaAction.RemoveTabMediaAction(
                    store.state.customTabs.map { it.id }
                ))

            is CustomTabListAction.RemoveCustomTabAction ->
                store.dispatch(MediaAction.RemoveTabMediaAction(
                    listOf(action.tabId)
                ))
        }
    }

    private fun postProcess(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        action: BrowserAction
    ) {
        when (action) {
            is MediaAction.AddMediaAction,
            is MediaAction.RemoveMediaAction,
            is MediaAction.RemoveTabMediaAction,
            is MediaAction.UpdateMediaVolumeAction,
            is MediaAction.UpdateMediaStateAction,
            is MediaAction.UpdateMediaFullscreenAction -> {
                mediaAggregateUpdate.process(context.store)
            }

            is MediaAction.UpdateMediaAggregateAction -> {
                mediaServiceLauncher.process(context.state)
                mediaFactsEmitter.process(context.state)
            }
        }
    }
}
