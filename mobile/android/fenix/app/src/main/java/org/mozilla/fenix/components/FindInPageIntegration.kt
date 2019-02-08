/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import android.util.AttributeSet
import android.view.View
import androidx.coordinatorlayout.widget.CoordinatorLayout
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.feature.findinpage.FindInPageFeature
import mozilla.components.feature.findinpage.view.FindInPageBar
import mozilla.components.feature.findinpage.view.FindInPageView
import mozilla.components.support.base.feature.BackHandler
import mozilla.components.support.base.feature.LifecycleAwareFeature

class FindInPageIntegration(
    private val sessionManager: SessionManager,
    private val view: FindInPageView
) : LifecycleAwareFeature, BackHandler {
    private val feature = FindInPageFeature(sessionManager, view, ::onClose)

    override fun start() {
        feature.start()

        FindInPageIntegration.launch = this::launch
    }

    override fun stop() {
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
        // This is a workaround to let the menu item find this integration and active "Find in Page" mode. That's a bit
        // ridiculous and there's no need that we create the toolbar menu items at app start time. Instead the
        // ToolbarIntegration should create them and get the FindInPageIntegration injected as a dependency if the
        // menu items need them.
        var launch: (() -> Unit)? = null
            private set
    }
}

/**
 * [CoordinatorLayout.Behavior] that will always position the [FindInPageBar] above the [BrowserToolbar] (including
 * when the browser toolbar is scrolling or performing a snap animation).
 */
@Suppress("unused") // Referenced from XML
class FindInPageBarBehavior(
    context: Context,
    attrs: AttributeSet
) : CoordinatorLayout.Behavior<FindInPageBar>(context, attrs) {
    override fun layoutDependsOn(parent: CoordinatorLayout, child: FindInPageBar, dependency: View): Boolean {
        if (dependency is BrowserToolbar) {
            return true
        }

        return super.layoutDependsOn(parent, child, dependency)
    }

    override fun onDependentViewChanged(parent: CoordinatorLayout, child: FindInPageBar, dependency: View): Boolean {
        return if (dependency is BrowserToolbar) {
            repositionFindInPageBar(child, dependency)
            true
        } else {
            false
        }
    }

    private fun repositionFindInPageBar(findInPageView: FindInPageBar, toolbar: BrowserToolbar) {
        findInPageView.translationY = (toolbar.translationY + toolbar.height * -1.0).toFloat()
    }
}
