/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import android.view.View
import com.google.android.material.appbar.AppBarLayout
import com.google.android.material.appbar.AppBarLayout.LayoutParams.SCROLL_FLAG_ENTER_ALWAYS
import com.google.android.material.appbar.AppBarLayout.LayoutParams.SCROLL_FLAG_EXIT_UNTIL_COLLAPSED
import com.google.android.material.appbar.AppBarLayout.LayoutParams.SCROLL_FLAG_SCROLL
import com.google.android.material.appbar.AppBarLayout.LayoutParams.SCROLL_FLAG_SNAP
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.mapNotNull
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineView
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged

/**
 * Feature implementation for connecting an [EngineView] with any View that you want to coordinate scrolling
 * behavior with.
 *
 * A use case could be collapsing a toolbar every time that the user scrolls.
 */
class CoordinateScrollingFeature(
    private val store: BrowserStore,
    private val engineView: EngineView,
    private val view: View,
    private val scrollFlags: Int = DEFAULT_SCROLL_FLAGS,
) : LifecycleAwareFeature {
    private var scope: CoroutineScope? = null

    /**
     * Start feature: Starts adding scrolling behavior for the indicated view.
     */
    override fun start() {
        scope = store.flowScoped { flow ->
            flow.mapNotNull { state -> state.selectedTab }
                .map { tab -> tab.content.loading }
                .ifChanged()
                .collect { onLoadingStateChanged() }
        }
    }

    override fun stop() {
        scope?.cancel()
    }

    private fun onLoadingStateChanged() {
        val params = view.layoutParams as AppBarLayout.LayoutParams

        if (engineView.canScrollVerticallyDown()) {
            params.scrollFlags = scrollFlags
        } else {
            params.scrollFlags = 0
        }

        view.layoutParams = params
    }

    companion object {
        const val DEFAULT_SCROLL_FLAGS = SCROLL_FLAG_SCROLL or
            SCROLL_FLAG_ENTER_ALWAYS or
            SCROLL_FLAG_SNAP or
            SCROLL_FLAG_EXIT_UNTIL_COLLAPSED
    }
}
