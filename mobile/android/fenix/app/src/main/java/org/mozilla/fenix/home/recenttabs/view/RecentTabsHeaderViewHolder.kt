/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recenttabs.view

import android.view.View
import kotlinx.android.synthetic.main.recent_tabs_header.*
import org.mozilla.fenix.R
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
        show_all_button.setOnClickListener {
            interactor.onRecentTabShowAllClicked()
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.recent_tabs_header
    }
}
