/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentbookmarks.view

import android.view.View
import androidx.navigation.findNavController
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.RecentBookmarksHeaderBinding
import org.mozilla.fenix.home.recentbookmarks.interactor.RecentBookmarksInteractor
import org.mozilla.fenix.utils.view.ViewHolder

/**
 * View holder for the recent bookmarks header and "Show all" button.
 *
 * @param view The container [View] for this view holder.
 * @param interactor [RecentBookmarksInteractor] which will have delegated to all user interactions.
 */
class RecentBookmarksHeaderViewHolder(
    view: View,
    private val interactor: RecentBookmarksInteractor
) : ViewHolder(view) {

    init {
        val binding = RecentBookmarksHeaderBinding.bind(view)
        binding.showAllBookmarksButton.setOnClickListener {
            dismissSearchDialogIfDisplayed()
            interactor.onShowAllBookmarksClicked()
        }
    }

    private fun dismissSearchDialogIfDisplayed() {
        val navController = itemView.findNavController()
        if (navController.currentDestination?.id == R.id.searchDialogFragment) {
            navController.navigateUp()
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.recent_bookmarks_header
    }
}
