/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentvisits.view

import android.view.View
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.RecentVisitsHeaderBinding
import org.mozilla.fenix.home.recentvisits.interactor.RecentVisitsInteractor
import org.mozilla.fenix.utils.view.ViewHolder

/**
 * View holder for the "Recent visits" section header with the "Show all" button.
 *
 * @property interactor [RecentVisitsInteractor] which will have delegated to all user
 * interactions.
 */
class RecentVisitsHeaderViewHolder(
    view: View,
    private val interactor: RecentVisitsInteractor
) : ViewHolder(view) {

    init {
        val binding = RecentVisitsHeaderBinding.bind(view)
        binding.showAllButton.setOnClickListener {
            interactor.onHistoryShowAllClicked()
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.recent_visits_header
    }
}
