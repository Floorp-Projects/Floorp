/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser.integration

import android.arch.lifecycle.Lifecycle
import android.arch.lifecycle.LifecycleObserver
import android.arch.lifecycle.OnLifecycleEvent
import android.view.View
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.findinpage.FindInPageFeature
import mozilla.components.feature.findinpage.view.FindInPageView
import mozilla.components.support.base.feature.BackHandler

class FindInPageIntegration(
    private val sessionManager: SessionManager,
    private val view: FindInPageView
) : LifecycleObserver, BackHandler {
    private val feature = FindInPageFeature(sessionManager, view, ::onClose)

    @OnLifecycleEvent(Lifecycle.Event.ON_START)
    fun onStart() {
        feature.start()

        FindInPageIntegration.launch = this::launch
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_STOP)
    fun onStop() {
        feature.stop()

        FindInPageIntegration.launch = null
    }

    override fun onBackPressed(): Boolean {
        return feature.onBackPressed()
    }

    private fun onClose() {
        view.asView().visibility = View.GONE
    }

    private fun launch() {
        val session = sessionManager.selectedSession ?: return

        view.asView().visibility = View.VISIBLE
        feature.bind(session)
    }

    companion object {
        var launch: (() -> Unit)? = null
            private set
    }
}
