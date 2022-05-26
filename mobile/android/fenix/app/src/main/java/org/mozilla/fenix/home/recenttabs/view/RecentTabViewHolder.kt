/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recenttabs.view

import android.view.View
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.lifecycle.LifecycleOwner
import mozilla.components.lib.state.ext.observeAsComposableState
import org.mozilla.fenix.R
import org.mozilla.fenix.components.components
import org.mozilla.fenix.compose.ComposeViewHolder
import org.mozilla.fenix.home.recentsyncedtabs.RecentSyncedTabState
import org.mozilla.fenix.home.recenttabs.interactor.RecentTabInteractor
import org.mozilla.fenix.home.recentsyncedtabs.interactor.RecentSyncedTabInteractor
import org.mozilla.fenix.home.recentsyncedtabs.view.RecentSyncedTab

/**
 * View holder for a recent tab item.
 *
 * @param composeView [ComposeView] which will be populated with Jetpack Compose UI content.
 * @param recentTabInteractor [RecentTabInteractor] which will have delegated to all user recent
 * tab interactions.
 * @param recentSyncedTabInteractor [RecentSyncedTabInteractor] which will have delegated to all user
 * recent synced tab interactions.
 */
class RecentTabViewHolder(
    composeView: ComposeView,
    viewLifecycleOwner: LifecycleOwner,
    private val recentTabInteractor: RecentTabInteractor,
    private val recentSyncedTabInteractor: RecentSyncedTabInteractor,
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
        val recentTabs = components.appStore.observeAsComposableState { state -> state.recentTabs }
        val recentSyncedTabState = components.appStore.observeAsComposableState { state -> state.recentSyncedTabState }

        Column {
            RecentTabs(
                recentTabs = recentTabs.value ?: emptyList(),
                onRecentTabClick = { recentTabInteractor.onRecentTabClicked(it) },
                onRecentSearchGroupClick = { recentTabInteractor.onRecentSearchGroupClicked(it) },
                menuItems = listOf(
                    RecentTabMenuItem(
                        title = stringResource(id = R.string.recent_tab_menu_item_remove),
                        onClick = { tab -> recentTabInteractor.onRemoveRecentTab(tab) }
                    )
                )
            )

            recentSyncedTabState.value?.let {
                if (components.settings.enableTaskContinuityEnhancements && it != RecentSyncedTabState.None) {
                    Spacer(modifier = Modifier.height(8.dp))

                    val syncedTab = when (it) {
                        RecentSyncedTabState.None,
                        RecentSyncedTabState.Loading -> null
                        is RecentSyncedTabState.Success -> it.tab
                    }
                    RecentSyncedTab(
                        tab = syncedTab,
                        onRecentSyncedTabClick = { tab ->
                            recentSyncedTabInteractor.onRecentSyncedTabClicked(tab)
                        },
                        onSeeAllSyncedTabsButtonClick = {
                            recentSyncedTabInteractor.onSyncedTabShowAllClicked()
                        },
                    )
                }
            }
        }
    }
}
