/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentvisits.view

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
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.ComposeViewHolder
import org.mozilla.fenix.compose.home.HomeSectionHeader
import org.mozilla.fenix.home.recentvisits.interactor.RecentVisitsInteractor

/**
 * View holder for the "Recent visits" section header with the "Show all" button.
 *
 * @param composeView [ComposeView] which will be populated with Jetpack Compose UI content.
 * @param viewLifecycleOwner [LifecycleOwner] to which this Composable will be tied to.
 * @property interactor [RecentVisitsInteractor] which will have delegated to all user
 * interactions.
 */
class RecentVisitsHeaderViewHolder(
    composeView: ComposeView,
    viewLifecycleOwner: LifecycleOwner,
    private val interactor: RecentVisitsInteractor,
) : ComposeViewHolder(composeView, viewLifecycleOwner) {

    init {
        val horizontalPadding =
            composeView.resources.getDimensionPixelSize(R.dimen.home_item_horizontal_margin)
        composeView.setPadding(horizontalPadding, 0, horizontalPadding, 0)
    }

    @Composable
    override fun Content() {
        Column {
            Spacer(modifier = Modifier.height(40.dp))

            HomeSectionHeader(
                headerText = stringResource(R.string.history_metadata_header_2),
                description = stringResource(R.string.past_explorations_show_all_content_description_2),
                onShowAllClick = interactor::onHistoryShowAllClicked,
            )

            Spacer(Modifier.height(16.dp))
        }
    }

    companion object {
        val LAYOUT_ID = View.generateViewId()
    }
}
