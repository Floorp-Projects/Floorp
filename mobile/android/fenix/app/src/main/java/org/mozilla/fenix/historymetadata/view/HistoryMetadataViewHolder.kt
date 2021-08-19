/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.historymetadata.view

import android.view.View
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.state.state.ContentState
import mozilla.components.concept.storage.HistoryMetadata
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.HistoryMetadataListRowBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.loadIntoView
import org.mozilla.fenix.historymetadata.interactor.HistoryMetadataInteractor
import org.mozilla.fenix.utils.view.ViewHolder

/**
 * View holder for a history metadata item.
 *
 * @property interactor [HistoryMetadataInteractor] which will have delegated to all user
 * interactions.
 * @property icons an instance of [BrowserIcons] for rendering the sites icon if one isn't found
 * in [ContentState.icon].
 */
class HistoryMetadataViewHolder(
    view: View,
    private val interactor: HistoryMetadataInteractor,
    private val icons: BrowserIcons = view.context.components.core.icons
) : ViewHolder(view) {

    val binding = HistoryMetadataListRowBinding.bind(view)

    fun bind(historyMetadata: HistoryMetadata) {
        binding.historyMetadataTitle.text = if (historyMetadata.title.isNullOrEmpty()) {
            historyMetadata.key.url
        } else {
            historyMetadata.title
        }

        icons.loadIntoView(binding.historyMetadataIcon, historyMetadata.key.url)

        itemView.setOnClickListener {
            interactor.onHistoryMetadataItemClicked(historyMetadata.key.url, historyMetadata.key)
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.history_metadata_list_row
    }
}
