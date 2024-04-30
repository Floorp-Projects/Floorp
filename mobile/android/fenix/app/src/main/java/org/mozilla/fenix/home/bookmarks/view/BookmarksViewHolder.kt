/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.bookmarks.view

import android.view.View
import androidx.compose.runtime.Composable
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.res.stringResource
import androidx.lifecycle.LifecycleOwner
import mozilla.components.lib.state.ext.observeAsComposableState
import mozilla.components.service.glean.private.NoExtras
import org.mozilla.fenix.GleanMetrics.HomeBookmarks
import org.mozilla.fenix.R
import org.mozilla.fenix.components.components
import org.mozilla.fenix.compose.ComposeViewHolder
import org.mozilla.fenix.home.bookmarks.interactor.BookmarksInteractor
import org.mozilla.fenix.wallpapers.WallpaperState

/**
 * ViewHolder for the Bookmarks section in the HomeFragment.
 */
class BookmarksViewHolder(
    composeView: ComposeView,
    viewLifecycleOwner: LifecycleOwner,
    val interactor: BookmarksInteractor,
) : ComposeViewHolder(composeView, viewLifecycleOwner) {

    init {
        HomeBookmarks.shown.record(NoExtras())
    }

    companion object {
        val LAYOUT_ID = View.generateViewId()
    }

    @Composable
    override fun Content() {
        val bookmarks = components.appStore.observeAsComposableState { state -> state.bookmarks }
        val wallpaperState = components.appStore
            .observeAsComposableState { state -> state.wallpaperState }.value ?: WallpaperState.default

        Bookmarks(
            bookmarks = bookmarks.value ?: emptyList(),
            backgroundColor = wallpaperState.wallpaperCardColor,
            onBookmarkClick = interactor::onBookmarkClicked,
            menuItems = listOf(
                BookmarksMenuItem(
                    stringResource(id = R.string.home_bookmarks_menu_item_remove),
                    onClick = { bookmark -> interactor.onBookmarkRemoved(bookmark) },
                ),
            ),
        )
    }
}
