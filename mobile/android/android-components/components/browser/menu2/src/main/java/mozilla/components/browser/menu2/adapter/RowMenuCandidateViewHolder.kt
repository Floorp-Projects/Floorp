/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2.adapter

import android.view.LayoutInflater
import android.view.View
import android.widget.LinearLayout
import androidx.annotation.LayoutRes
import mozilla.components.browser.menu2.R
import mozilla.components.concept.menu.candidate.RowMenuCandidate

/**
 * Displays a row of small menu options.
 */
internal class RowMenuCandidateViewHolder(
    itemView: View,
    inflater: LayoutInflater,
    private val dismiss: () -> Unit,
) : MenuCandidateViewHolder<RowMenuCandidate>(itemView, inflater) {

    private val layout = itemView as LinearLayout
    private var buttonViewHolders = emptyList<SmallMenuCandidateViewHolder>()

    override fun bind(newCandidate: RowMenuCandidate, oldCandidate: RowMenuCandidate?) {
        super.bind(newCandidate, oldCandidate)

        // If the number of children in the row changes,
        // build new holders for each of them.
        if (newCandidate.items.size != oldCandidate?.items?.size) {
            layout.removeAllViews()
            // Create new view holders list
            buttonViewHolders = newCandidate.items.map {
                val button = inflater.inflate(
                    SmallMenuCandidateViewHolder.layoutResource,
                    layout,
                    false,
                )
                layout.addView(button)
                SmallMenuCandidateViewHolder(button, dismiss)
            }
        }

        // Use the button view holders to compare individual menu items in the row.
        buttonViewHolders.zip(newCandidate.items).forEach { (viewHolder, item) ->
            viewHolder.bind(item)
        }
    }

    companion object {
        @LayoutRes
        val layoutResource = R.layout.mozac_browser_menu2_candidate_row
    }
}
