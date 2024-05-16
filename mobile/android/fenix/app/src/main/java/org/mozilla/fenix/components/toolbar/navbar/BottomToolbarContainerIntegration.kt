/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar.navbar

import androidx.annotation.VisibleForTesting
import androidx.core.view.isVisible
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.distinctUntilChangedBy
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.toolbar.ScrollableToolbar
import mozilla.components.feature.toolbar.ToolbarBehaviorController
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature
import org.mozilla.fenix.components.AppStore

/**
 * The feature responsible for scrolling behaviour of the bottom toolbar container.
 * When the content of a tab is being scrolled, the toolbar will react to user interactions.
 */
class BottomToolbarContainerIntegration(
    val toolbar: ScrollableToolbar,
    val store: BrowserStore,
    val appStore: AppStore,
    val bottomToolbarContainerView: BottomToolbarContainerView,
    sessionId: String?,
) : LifecycleAwareFeature {

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    var toolbarController = ToolbarBehaviorController(toolbar, store, sessionId)
    private var scope: CoroutineScope? = null

    override fun start() {
        toolbarController.start()

        scope = appStore.flowScoped { flow ->
            flow.distinctUntilChangedBy { it.isSearchDialogVisible }
                .collect { state ->
                    bottomToolbarContainerView.composeView.isVisible = !state.isSearchDialogVisible
                }
        }
    }

    override fun stop() {
        toolbarController.stop()
        scope?.cancel()
    }
}
