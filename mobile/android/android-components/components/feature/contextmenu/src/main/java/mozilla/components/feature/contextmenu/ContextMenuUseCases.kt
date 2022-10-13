/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.contextmenu

import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.ShareInternetResourceAction
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.state.content.ShareInternetResourceState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.HitResult

/**
 * Contains use cases related to the context menu feature.
 *
 * @param store the application's [BrowserStore].
 */
class ContextMenuUseCases(
    store: BrowserStore,
) {
    class ConsumeHitResultUseCase(
        private val store: BrowserStore,
    ) {
        /**
         * Consumes the [HitResult] from the [BrowserStore] with the given [tabId].
         */
        operator fun invoke(tabId: String) {
            store.dispatch(ContentAction.ConsumeHitResultAction(tabId))
        }
    }

    class InjectDownloadUseCase(
        private val store: BrowserStore,
    ) {
        /**
         * Adds a [DownloadState] to the [BrowserStore] with the given [tabId].
         *
         * This is a hacky workaround. After we have migrated everything from browser-session to
         * browser-state we should revisits this and find a better solution.
         */
        operator fun invoke(tabId: String, download: DownloadState) {
            store.dispatch(
                ContentAction.UpdateDownloadAction(
                    tabId,
                    download,
                ),
            )
        }
    }

    /**
     * Usecase allowing adding a new [ShareInternetResourceState] to the [BrowserStore]
     */
    class InjectShareInternetResourceUseCase(
        private val store: BrowserStore,
    ) {
        /**
         * Adds a specific [ShareInternetResourceState] to the [BrowserStore].
         */
        operator fun invoke(tabId: String, internetResource: ShareInternetResourceState) {
            store.dispatch(ShareInternetResourceAction.AddShareAction(tabId, internetResource))
        }
    }

    val consumeHitResult = ConsumeHitResultUseCase(store)
    val injectDownload = InjectDownloadUseCase(store)
    val injectShareFromInternet = InjectShareInternetResourceUseCase(store)
}
