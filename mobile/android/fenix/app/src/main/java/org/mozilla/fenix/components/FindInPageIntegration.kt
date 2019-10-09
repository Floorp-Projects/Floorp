/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.view.View
import android.view.ViewStub
import mozilla.components.browser.state.selector.findCustomTabOrSelectedTab
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.engine.EngineView
import mozilla.components.feature.findinpage.FindInPageFeature
import mozilla.components.feature.findinpage.view.FindInPageView
import mozilla.components.support.base.feature.LifecycleAwareFeature
import org.mozilla.fenix.test.Mockable

@Mockable
class FindInPageIntegration(
    private val store: BrowserStore,
    private val sessionId: String? = null,
    stub: ViewStub,
    private val engineView: EngineView,
    private val toolbar: BrowserToolbar
) : InflationAwareFeature(stub) {
    override fun onViewInflated(view: View): LifecycleAwareFeature {
        return FindInPageFeature(store, view as FindInPageView, engineView) {
            toolbar.visibility = View.VISIBLE
            view.visibility = View.GONE
        }
    }

    override fun onLaunch(view: View, feature: LifecycleAwareFeature) {
        store.state.findCustomTabOrSelectedTab(sessionId)?.let { tab ->
            if (tab !is CustomTabSessionState) {
                // Hide the toolbar to display find in page query (only
                // needs to be done for regular tabs with bottom toolbar).
                toolbar.visibility = View.GONE
            }
            view.visibility = View.VISIBLE
            (feature as FindInPageFeature).bind(tab)
            view.layoutParams.height = toolbar.height
        }
    }
}
