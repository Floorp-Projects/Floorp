/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recenttabs.view

import android.view.View
import androidx.navigation.findNavController
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.RecentTabsHeaderBinding
import org.mozilla.fenix.home.recenttabs.interactor.RecentTabInteractor
import org.mozilla.fenix.utils.view.ViewHolder

/**
 * View holder for the recent tabs header and "Show all" button.
 *
 * @param interactor [RecentTabInteractor] which will have delegated to all user interactions.
 */
class RecentTabsHeaderViewHolder(
    view: View,
    private val interactor: RecentTabInteractor
) : ViewHolder(view) {

    init {

        val binding = RecentTabsHeaderBinding.bind(view)
        binding.showAllButton.setOnClickListener {
            dismissSearchDialogIfDisplayed()
            interactor.onRecentTabShowAllClicked()
        }
    }

    private fun dismissSearchDialogIfDisplayed() {
        val navController = itemView.findNavController()
        if (navController.currentDestination?.id == R.id.searchDialogFragment) {
            navController.navigateUp()
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.recent_tabs_header
    }
}
