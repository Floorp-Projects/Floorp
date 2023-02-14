/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentbookmarks.view

import android.view.View
import androidx.compose.runtime.Composable
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.res.stringResource
import androidx.lifecycle.LifecycleOwner
import mozilla.components.lib.state.ext.observeAsComposableState
import mozilla.components.service.glean.private.NoExtras
import org.mozilla.fenix.R
import org.mozilla.fenix.components.components
import org.mozilla.fenix.compose.ComposeViewHolder
import org.mozilla.fenix.home.recentbookmarks.interactor.RecentBookmarksInteractor
import org.mozilla.fenix.wallpapers.WallpaperState
import org.mozilla.fenix.GleanMetrics.RecentBookmarks as RecentBookmarksMetrics

class RecentBookmarksViewHolder(
    composeView: ComposeView,
    viewLifecycleOwner: LifecycleOwner,
    val interactor: RecentBookmarksInteractor,
) : ComposeViewHolder(composeView, viewLifecycleOwner) {

    init {
        RecentBookmarksMetrics.shown.record(NoExtras())
    }

    companion object {
        val LAYOUT_ID = View.generateViewId()
    }

    @Composable
    override fun Content() {
        val recentBookmarks = components.appStore.observeAsComposableState { state -> state.recentBookmarks }
        val wallpaperState = components.appStore
            .observeAsComposableState { state -> state.wallpaperState }.value ?: WallpaperState.default

        RecentBookmarks(
            bookmarks = recentBookmarks.value ?: emptyList(),
            backgroundColor = wallpaperState.wallpaperCardColor,
            onRecentBookmarkClick = interactor::onRecentBookmarkClicked,
            menuItems = listOf(
                RecentBookmarksMenuItem(
                    stringResource(id = R.string.recently_saved_menu_item_remove),
                    onClick = { bookmark -> interactor.onRecentBookmarkRemoved(bookmark) },
                ),
            ),
        )
    }
}
