/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.nimbus.ui

import android.view.View
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView

/**
 * A view holder for displaying details of a Nimbus experiment.
 */
class NimbusDetailItemViewHolder(
    view: View,
    private val titleView: TextView,
    private val summaryView: TextView
) : RecyclerView.ViewHolder(view) {

    internal fun bind(branch: String) {
        titleView.text = branch
        summaryView.text = branch
    }
}
