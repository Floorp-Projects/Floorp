/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentbookmarks.view

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
import org.mozilla.fenix.home.recentbookmarks.interactor.RecentBookmarksInteractor

/**
 * View holder for the recent bookmarks header and "Show all" button.
 *
 * @param composeView [ComposeView] which will be populated with Jetpack Compose UI content.
 * @param viewLifecycleOwner [LifecycleOwner] life cycle owner for the view.
 * @property interactor [RecentBookmarksInteractor] which will have delegated to all user interactions.
 */
class RecentBookmarksHeaderViewHolder(
    composeView: ComposeView,
    viewLifecycleOwner: LifecycleOwner,
    private val interactor: RecentBookmarksInteractor,
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
                headerText = stringResource(R.string.recently_saved_title),
                description = stringResource(R.string.recently_saved_show_all_content_description_2),
                onShowAllClick = {
                    interactor.onShowAllBookmarksClicked()
                },
            )

            Spacer(Modifier.height(16.dp))
        }
    }

    companion object {
        val LAYOUT_ID = View.generateViewId()
    }
}
