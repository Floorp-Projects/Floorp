/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import android.os.Looper
import android.util.AttributeSet
import android.view.View
import android.view.ViewStub
import androidx.annotation.UiThread
import androidx.coordinatorlayout.widget.CoordinatorLayout
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.runWithSessionIdOrSelected
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.engine.EngineView
import mozilla.components.feature.findinpage.FindInPageFeature
import mozilla.components.feature.findinpage.view.FindInPageBar
import mozilla.components.feature.findinpage.view.FindInPageView
import mozilla.components.support.base.feature.BackHandler
import mozilla.components.support.base.feature.LifecycleAwareFeature
import org.mozilla.fenix.test.Mockable

/**
 * This class provides lazy loading of the Find in Page View.
 * It should be launched on the app's UI thread.
 */
@Mockable
class FindInPageIntegration(
    private val sessionManager: SessionManager,
    private val sessionId: String? = null,
    private val stub: ViewStub,
    engineView: EngineView,
    private val toolbar: BrowserToolbar
) : LifecycleAwareFeature, BackHandler {

    private var view: FindInPageView? = null
    private val feature: FindInPageFeature by lazy(LazyThreadSafetyMode.NONE) {
        view = stub.inflate() as FindInPageView
        FindInPageFeature(sessionManager, view!!, engineView, ::onClose).also { it.start() }
    }

    override fun start() {
    }

    override fun stop() {
        if (view != null) feature.stop()
    }

    override fun onBackPressed(): Boolean {
        return if (view != null) feature.onBackPressed() else false
    }

    private fun onClose() {
        toolbar.visibility = View.VISIBLE
        view?.asView()?.visibility = View.GONE
    }

    @UiThread
    fun launch() {
        require(Looper.myLooper() == Looper.getMainLooper()) { "This method should be run on the main UI thread." }
        sessionManager.runWithSessionIdOrSelected(sessionId) {
            if (!it.isCustomTabSession()) {
                toolbar.visibility = View.GONE
            }
            feature.bind(it)
            view?.asView()?.visibility = View.VISIBLE
        }
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
    override fun layoutDependsOn(
        parent: CoordinatorLayout,
        child: FindInPageBar,
        dependency: View
    ): Boolean {
        if (dependency is BrowserToolbar) {
            return true
        }

        return super.layoutDependsOn(parent, child, dependency)
    }

    override fun onDependentViewChanged(
        parent: CoordinatorLayout,
        child: FindInPageBar,
        dependency: View
    ): Boolean {
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
