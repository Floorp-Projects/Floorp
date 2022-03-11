/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recenttabs.view

import android.view.View
import androidx.compose.runtime.Composable
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.res.stringResource
import androidx.lifecycle.LifecycleOwner
import mozilla.components.lib.state.ext.observeAsComposableState
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.ComposeViewHolder
import org.mozilla.fenix.home.HomeFragmentStore
import org.mozilla.fenix.home.recenttabs.interactor.RecentTabInteractor

/**
 * View holder for a recent tab item.
 *
 * @param composeView [ComposeView] which will be populated with Jetpack Compose UI content.
 * @param store [HomeFragmentStore] containing the list of recent tabs to be displayed.
 * @param interactor [RecentTabInteractor] which will have delegated to all user interactions.
 */
class RecentTabViewHolder(
    composeView: ComposeView,
    viewLifecycleOwner: LifecycleOwner,
    private val store: HomeFragmentStore,
    private val interactor: RecentTabInteractor
) : ComposeViewHolder(composeView, viewLifecycleOwner) {

    init {
        val horizontalPadding =
            composeView.resources.getDimensionPixelSize(R.dimen.home_item_horizontal_margin)
        composeView.setPadding(horizontalPadding, 0, horizontalPadding, 0)
    }

    companion object {
        val LAYOUT_ID = View.generateViewId()
    }

    @Composable
    override fun Content() {
        val recentTabs = store.observeAsComposableState { state -> state.recentTabs }

        RecentTabs(
            recentTabs = recentTabs.value ?: emptyList(),
            onRecentTabClick = { interactor.onRecentTabClicked(it) },
            onRecentSearchGroupClick = { interactor.onRecentSearchGroupClicked(it) },
            menuItems = listOf(
                RecentTabMenuItem(
                    title = stringResource(id = R.string.recent_tab_menu_item_remove),
                    onClick = { tab -> interactor.onRemoveRecentTab(tab) }
                )
            )
        )
    }
}
