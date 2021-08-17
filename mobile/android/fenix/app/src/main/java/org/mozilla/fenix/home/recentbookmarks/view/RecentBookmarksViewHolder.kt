/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentbookmarks.view

import android.view.View
import androidx.navigation.findNavController
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.LinearLayoutManager.HORIZONTAL
import mozilla.components.concept.storage.BookmarkNode
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.ComponentRecentBookmarksBinding
import org.mozilla.fenix.home.recentbookmarks.RecentBookmarksItemAdapter
import org.mozilla.fenix.home.recentbookmarks.interactor.RecentBookmarksInteractor
import org.mozilla.fenix.utils.view.ViewHolder

class RecentBookmarksViewHolder(
    view: View,
    val interactor: RecentBookmarksInteractor
) : ViewHolder(view) {

    private val recentBookmarksAdapter = RecentBookmarksItemAdapter(interactor)

    init {
        val recentBookmarksBinding = ComponentRecentBookmarksBinding.bind(view)
        val recentBookmarksHeaderBinding = recentBookmarksBinding.recentBookmarksHeader

        val linearLayoutManager = LinearLayoutManager(view.context, HORIZONTAL, false)

        recentBookmarksBinding.recentBookmarksList.apply {
            adapter = recentBookmarksAdapter
            layoutManager = linearLayoutManager
        }

        recentBookmarksHeaderBinding.showAllBookmarksButton.setOnClickListener {
            dismissSearchDialogIfDisplayed()
            interactor.onShowAllBookmarksClicked()
        }
    }

    fun bind(bookmarks: List<BookmarkNode>) {
        recentBookmarksAdapter.submitList(bookmarks)
    }

    private fun dismissSearchDialogIfDisplayed() {
        val navController = itemView.findNavController()
        if (navController.currentDestination?.id == R.id.searchDialogFragment) {
            navController.navigateUp()
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.component_recent_bookmarks
    }
}
