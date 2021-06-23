/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.historymetadata.view

import android.view.View
import kotlinx.android.synthetic.main.history_metadata_group.*
import org.mozilla.fenix.R
import org.mozilla.fenix.historymetadata.HistoryMetadataGroup
import org.mozilla.fenix.historymetadata.interactor.HistoryMetadataInteractor
import org.mozilla.fenix.utils.view.ViewHolder

/**
 * View holder for a history metadata group item.
 *
 * @property interactor [HistoryMetadataInteractor] which will have delegated to all user
 * interactions.
 */
class HistoryMetadataGroupViewHolder(
    view: View,
    private val interactor: HistoryMetadataInteractor
) : ViewHolder(view) {

    fun bind(historyMetadataGroup: HistoryMetadataGroup) {
        history_metadata_group_title.text = historyMetadataGroup.title

        itemView.isActivated = historyMetadataGroup.expanded

        itemView.setOnClickListener {
            interactor.onToggleHistoryMetadataGroupExpanded(historyMetadataGroup)
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.history_metadata_group
    }
}
