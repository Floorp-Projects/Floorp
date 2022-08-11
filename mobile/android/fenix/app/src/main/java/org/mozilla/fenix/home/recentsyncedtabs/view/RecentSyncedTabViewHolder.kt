/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentsyncedtabs.view

import android.view.View
import androidx.compose.runtime.Composable
import androidx.compose.ui.platform.ComposeView
import androidx.lifecycle.LifecycleOwner
import mozilla.components.lib.state.ext.observeAsComposableState
import org.mozilla.fenix.R
import org.mozilla.fenix.components.components
import org.mozilla.fenix.compose.ComposeViewHolder
import org.mozilla.fenix.home.recentsyncedtabs.RecentSyncedTabState
import org.mozilla.fenix.home.recentsyncedtabs.interactor.RecentSyncedTabInteractor

/**
 * View holder for a recent synced tab item.
 *
 * @param composeView [ComposeView] which will be populated with Jetpack Compose UI content.
 * @param recentSyncedTabInteractor [RecentSyncedTabInteractor] which will have delegated to all
 * recent synced tab user interactions.
 */
class RecentSyncedTabViewHolder(
    composeView: ComposeView,
    viewLifecycleOwner: LifecycleOwner,
    private val recentSyncedTabInteractor: RecentSyncedTabInteractor,
) : ComposeViewHolder(composeView, viewLifecycleOwner) {

    init {
        val horizontalPadding =
            composeView.resources.getDimensionPixelSize(R.dimen.home_item_horizontal_margin)
        val verticalPadding =
            composeView.resources.getDimensionPixelSize(R.dimen.home_item_vertical_margin)
        composeView.setPadding(horizontalPadding, verticalPadding, horizontalPadding, 0)
    }

    companion object {
        val LAYOUT_ID = View.generateViewId()
    }

    @Composable
    override fun Content() {
        val recentSyncedTabState =
            components.appStore.observeAsComposableState { state -> state.recentSyncedTabState }
        recentSyncedTabState.value?.let {
            val syncedTab = when (it) {
                RecentSyncedTabState.None,
                RecentSyncedTabState.Loading -> null
                is RecentSyncedTabState.Success -> it.tabs.firstOrNull()
            }
            RecentSyncedTab(
                tab = syncedTab,
                onRecentSyncedTabClick = recentSyncedTabInteractor::onRecentSyncedTabClicked,
                onSeeAllSyncedTabsButtonClick = recentSyncedTabInteractor::onSyncedTabShowAllClicked,
            )
        }
    }
}
