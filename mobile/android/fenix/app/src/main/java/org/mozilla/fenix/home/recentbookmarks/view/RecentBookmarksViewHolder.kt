/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentbookmarks.view

import android.view.View
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.ViewCompositionStrategy
import mozilla.components.lib.state.ext.observeAsComposableState
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.home.HomeFragmentStore
import org.mozilla.fenix.home.recentbookmarks.interactor.RecentBookmarksInteractor
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.utils.view.ViewHolder

class RecentBookmarksViewHolder(
    val composeView: ComposeView,
    private val store: HomeFragmentStore,
    val interactor: RecentBookmarksInteractor,
    val metrics: MetricController
) : ViewHolder(composeView) {

    init {
        metrics.track(Event.RecentBookmarksShown)

        composeView.setViewCompositionStrategy(
            ViewCompositionStrategy.DisposeOnViewTreeLifecycleDestroyed
        )
        composeView.setContent {
            val recentBookmarks = store.observeAsComposableState { state -> state.recentBookmarks }

            FirefoxTheme {
                RecentBookmarks(
                    bookmarks = recentBookmarks.value ?: emptyList(),
                    onRecentBookmarkClick = interactor::onRecentBookmarkClicked
                )
            }
        }
    }

    companion object {
        val LAYOUT_ID = View.generateViewId()
    }
}
