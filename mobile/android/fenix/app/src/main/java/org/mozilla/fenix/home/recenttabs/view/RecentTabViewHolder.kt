/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recenttabs.view

import android.view.View
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.ViewCompositionStrategy
import mozilla.components.lib.state.ext.observeAsComposableState
import org.mozilla.fenix.home.HomeFragmentStore
import org.mozilla.fenix.home.recenttabs.interactor.RecentTabInteractor
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.utils.view.ViewHolder

/**
 * View holder for a recent tab item.
 *
 * @param composeView [ComposeView] which will be populated with Jetpack Compose UI content.
 * @param store [HomeFragmentStore] containing the list of recent tabs to be displayed.
 * @param interactor [RecentTabInteractor] which will have delegated to all user interactions.
 */
class RecentTabViewHolder(
    val composeView: ComposeView,
    private val store: HomeFragmentStore,
    private val interactor: RecentTabInteractor
) : ViewHolder(composeView) {

    init {
        composeView.setViewCompositionStrategy(
            ViewCompositionStrategy.DisposeOnViewTreeLifecycleDestroyed
        )
        composeView.setContent {
            val recentTabs = store.observeAsComposableState { state -> state.recentTabs }

            FirefoxTheme {
                RecentTabs(
                    recentTabs = recentTabs.value ?: emptyList(),
                    onRecentTabClick = { interactor.onRecentTabClicked(it) },
                    onRecentSearchGroupClicked = { interactor.onRecentSearchGroupClicked(it) }
                )
            }
        }
    }

    companion object {
        val LAYOUT_ID = View.generateViewId()
    }
}
