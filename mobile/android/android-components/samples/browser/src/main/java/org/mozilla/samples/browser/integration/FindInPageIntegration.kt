/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser.integration

import android.view.View
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineView
import mozilla.components.feature.findinpage.FindInPageFeature
import mozilla.components.feature.findinpage.view.FindInPageView
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.base.feature.UserInteractionHandler

@Suppress("UndocumentedPublicClass")
class FindInPageIntegration(
    private val store: BrowserStore,
    private val view: FindInPageView,
    engineView: EngineView,
) : LifecycleAwareFeature, UserInteractionHandler {
    private val feature = FindInPageFeature(store, view, engineView, ::onClose)

    override fun start() {
        feature.start()
        launch = this::launch
    }

    override fun stop() {
        feature.stop()
        launch = null
    }

    override fun onBackPressed(): Boolean {
        return feature.onBackPressed()
    }

    private fun onClose() {
        view.asView().visibility = View.GONE
    }

    private fun launch() {
        val session = store.state.selectedTab ?: return

        view.asView().visibility = View.VISIBLE
        feature.bind(session)
    }

    companion object {
        var launch: (() -> Unit)? = null
            private set
    }
}
