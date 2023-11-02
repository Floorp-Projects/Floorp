/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.view.View
import android.view.ViewGroup.MarginLayoutParams
import androidx.annotation.UiThread
import androidx.annotation.VisibleForTesting
import androidx.core.view.isVisible
import mozilla.components.browser.state.selector.findCustomTabOrSelectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.engine.EngineView
import mozilla.components.feature.findinpage.FindInPageFeature
import mozilla.components.feature.findinpage.view.FindInPageBar
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.base.feature.UserInteractionHandler
import org.mozilla.fenix.components.FindInPageIntegration.ToolbarInfo

/**
 * BrowserFragment delegate to handle all layout updates needed to show or hide the find in page bar.
 *
 * @param store The [BrowserStore] used to look up the current selected tab.
 * @param sessionId ID of the [store] session in which the query will be performed.
 * @param view The [FindInPageBar] view to display.
 * @param engineView the browser in which the queries will be made and which needs to be better positioned
 * to suit the find in page bar.
 * @param toolbarInfo [ToolbarInfo] used to configure the [BrowserToolbar] while the find in page bar is shown.
 */
class FindInPageIntegration(
    private val store: BrowserStore,
    private val sessionId: String? = null,
    private val view: FindInPageBar,
    private val engineView: EngineView,
    private val toolbarInfo: ToolbarInfo,
) : LifecycleAwareFeature, UserInteractionHandler {
    private val feature by lazy { FindInPageFeature(store, view, engineView, ::onClose) }

    override fun start() {
        feature.start()
    }

    override fun stop() {
        feature.stop()
    }

    override fun onBackPressed(): Boolean {
        return feature.onBackPressed()
    }

    private fun onClose() {
        view.visibility = View.GONE
        restorePreviousLayout()
    }

    /**
     * Start the find in page functionality.
     */
    @UiThread
    fun launch() {
        onLaunch(view, feature)
    }

    private fun onLaunch(view: View, feature: LifecycleAwareFeature) {
        store.state.findCustomTabOrSelectedTab(sessionId)?.let { tab ->
            prepareLayoutForFindBar()

            view.visibility = View.VISIBLE
            (feature as FindInPageFeature).bind(tab)
            view.layoutParams.height = toolbarInfo.toolbar.height
        }
    }

    @VisibleForTesting
    internal fun restorePreviousLayout() {
        toolbarInfo.toolbar.isVisible = true

        val engineViewParent = getEngineViewParent()
        val engineViewParentParams = getEngineViewsParentLayoutParams()
        if (toolbarInfo.isToolbarPlacedAtTop) {
            if (toolbarInfo.isToolbarDynamic) {
                engineViewParent.translationY = toolbarInfo.toolbar.height.toFloat()
                engineViewParentParams.bottomMargin = 0
            } else {
                engineViewParent.translationY = 0f
            }
        } else {
            if (toolbarInfo.isToolbarDynamic) {
                engineViewParentParams.bottomMargin = 0
            }
        }
    }

    @VisibleForTesting
    internal fun prepareLayoutForFindBar() {
        toolbarInfo.toolbar.isVisible = false

        val engineViewParent = getEngineViewParent()
        val engineViewParentParams = getEngineViewsParentLayoutParams()
        if (toolbarInfo.isToolbarPlacedAtTop) {
            if (toolbarInfo.isToolbarDynamic) {
                // With a dynamic toolbar the EngineView extends to the entire (top and bottom) of the screen.
                // And now with the toolbar expanded it is translated down immediately below the toolbar.
                engineViewParent.translationY = 0f
                engineViewParentParams.bottomMargin = toolbarInfo.toolbar.height
            } else {
                // With a fixed toolbar the EngineView is anchored below the toolbar with 0 Y translation.
                engineViewParent.translationY = -toolbarInfo.toolbar.height.toFloat()
            }
        } else {
            // With a bottom toolbar the EngineView is already anchored to the top of the screen.
            // Need just to ensure space for the find in page bar under the engineView.
            engineViewParentParams.bottomMargin = toolbarInfo.toolbar.height
        }
    }

    @VisibleForTesting
    internal fun getEngineViewParent() = engineView.asView().parent as View

    @VisibleForTesting
    internal fun getEngineViewsParentLayoutParams() = getEngineViewParent().layoutParams as MarginLayoutParams

    /**
     * Holder of all details needed about the Toolbar.
     * Used to modify the layout of BrowserToolbar while the find in page bar is shown.
     */
    data class ToolbarInfo(
        val toolbar: BrowserToolbar,
        val isToolbarDynamic: Boolean,
        val isToolbarPlacedAtTop: Boolean,
    )
}
