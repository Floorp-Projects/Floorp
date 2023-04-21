/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.browser.integration

import androidx.core.view.isVisible
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.engine.EngineView
import mozilla.components.feature.findinpage.FindInPageFeature
import mozilla.components.feature.findinpage.view.FindInPageBar
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.base.feature.UserInteractionHandler

class FindInPageIntegration(
    store: BrowserStore,
    private val findInPageView: FindInPageBar,
    private val browserToolbar: BrowserToolbar,
    engineView: EngineView,
) : LifecycleAwareFeature, UserInteractionHandler {
    private val feature = FindInPageFeature(
        store,
        findInPageView,
        engineView,
        ::hide,
    )

    override fun start() {
        feature.start()
    }

    override fun stop() {
        feature.stop()
    }

    override fun onBackPressed(): Boolean {
        return feature.onBackPressed()
    }

    fun show(sessionState: SessionState) {
        findInPageView.isVisible = true
        // Hiding the toolbar prevents Talkback from dictating its elements.
        browserToolbar.isVisible = false
        feature.bind(sessionState)
    }

    fun hide() {
        findInPageView.isVisible = false
        browserToolbar.isVisible = true
    }
}
