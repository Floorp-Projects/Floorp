/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.browser.integration

import android.view.View
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineView
import mozilla.components.feature.findinpage.FindInPageFeature
import mozilla.components.feature.findinpage.view.FindInPageBar
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.base.feature.UserInteractionHandler

class FindInPageIntegration(
    store: BrowserStore,
    private val findInPageView: FindInPageBar,
    engineView: EngineView
) : LifecycleAwareFeature, UserInteractionHandler {
    private val feature = FindInPageFeature(
        store,
        findInPageView,
        engineView,
        ::hide
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
        findInPageView.visibility = View.VISIBLE
        feature.bind(sessionState)
    }

    fun hide() {
        findInPageView.visibility = View.GONE
    }
}
