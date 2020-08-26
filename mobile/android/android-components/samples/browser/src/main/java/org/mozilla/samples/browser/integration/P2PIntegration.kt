/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser.integration

import android.view.View
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.p2p.P2PFeature
import mozilla.components.feature.p2p.view.P2PView
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.lib.nearby.NearbyConnection
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.base.feature.OnNeedToRequestPermissions

/**
 * Functionality to integrate a peer-to-peer communication toolbar in an application.
 */
@Suppress("LongParameterList")
class P2PIntegration(
    store: BrowserStore,
    engine: Engine,
    private val view: P2PView,
    thunk: () -> NearbyConnection,
    tabsUseCases: TabsUseCases,
    sessionUseCases: SessionUseCases,
    onNeedToRequestPermissions: OnNeedToRequestPermissions
) : LifecycleAwareFeature {
    val feature = P2PFeature(
        view,
        store,
        engine,
        thunk,
        tabsUseCases,
        sessionUseCases,
        onNeedToRequestPermissions,
        ::onClose
    )
    override fun start() {
        launch = this::launch
    }

    override fun stop() {
        launch = null
    }

    private fun onClose() {
        view.asView().visibility = View.GONE
    }

    private fun launch() {
        view.asView().visibility = View.VISIBLE
        feature.start()
    }

    companion object {
        var launch: (() -> Unit)? = null
            private set
    }
}
