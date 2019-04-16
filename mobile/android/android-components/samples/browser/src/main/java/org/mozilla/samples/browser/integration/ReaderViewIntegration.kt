/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.samples.browser.integration

import android.content.Context
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.readerview.ReaderViewFeature
import mozilla.components.feature.readerview.view.ReaderViewControlsView
import mozilla.components.support.base.feature.BackHandler
import mozilla.components.support.base.feature.LifecycleAwareFeature

class ReaderViewIntegration(
    context: Context,
    engine: Engine,
    sessionManager: SessionManager,
    private val view: ReaderViewControlsView
) : LifecycleAwareFeature, BackHandler {

    private val feature = ReaderViewFeature(context, engine, sessionManager, view)

    override fun start() {
        feature.start()

        ReaderViewIntegration.launch = this::launch
    }

    override fun stop() {
        feature.stop()
    }

    override fun onBackPressed(): Boolean {
        return feature.onBackPressed()
    }

    private fun launch() {
        view.showControls()
    }

    companion object {
        var launch: (() -> Unit)? = null
            private set
    }
}
