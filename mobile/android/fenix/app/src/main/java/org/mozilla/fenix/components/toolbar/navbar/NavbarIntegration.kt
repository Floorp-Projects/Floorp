/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar.navbar

import androidx.annotation.VisibleForTesting
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.toolbar.ScrollableToolbar
import mozilla.components.feature.toolbar.ToolbarBehaviorController
import mozilla.components.support.base.feature.LifecycleAwareFeature

/**
 * The feature responsible for scrolling behaviour of the navigation bar.
 * When the content of a tab is being scrolled, the nav bar will react
 * to the user interactions.
 */
class NavbarIntegration(
    val toolbar: ScrollableToolbar,
    val store: BrowserStore,
    sessionId: String?,
) : LifecycleAwareFeature {

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    var toolbarController = ToolbarBehaviorController(toolbar, store, sessionId)

    override fun start() {
        toolbarController.start()
    }

    override fun stop() {
        toolbarController.stop()
    }
}
