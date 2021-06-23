/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.historymetadata.view

import android.view.View
import kotlinx.android.synthetic.main.history_metadata_header.*
import org.mozilla.fenix.R
import org.mozilla.fenix.historymetadata.interactor.HistoryMetadataInteractor
import org.mozilla.fenix.utils.view.ViewHolder

/**
 * View holder for the history metadata header and "Show all" button.
 *
 * @property interactor [HistoryMetadataInteractor] which will have delegated to all user
 * interactions.
 */
class HistoryMetadataHeaderViewHolder(
    view: View,
    private val interactor: HistoryMetadataInteractor
) : ViewHolder(view) {

    init {
        show_all_button.setOnClickListener {
            interactor.onHistoryMetadataShowAllClicked()
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.history_metadata_header
    }
}
