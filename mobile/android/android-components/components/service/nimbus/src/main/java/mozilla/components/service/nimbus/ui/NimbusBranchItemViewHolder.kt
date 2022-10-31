/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.nimbus.ui

import android.view.View
import android.widget.ImageView
import android.widget.TextView
import androidx.core.view.isVisible
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.experiments.nimbus.Branch

/**
 * A view holder for displaying a branch of a Nimbus experiment.
 */
@Suppress("LongParameterList")
class NimbusBranchItemViewHolder(
    view: View,
    private val nimbusBranchesDelegate: NimbusBranchesAdapterDelegate,
    private val selectedIconView: ImageView,
    private val titleView: TextView,
    private val summaryView: TextView,
) : RecyclerView.ViewHolder(view) {

    internal fun bind(branch: Branch, selectedBranch: String) {
        selectedIconView.isVisible = selectedBranch == branch.slug
        titleView.text = branch.slug
        summaryView.text = branch.slug

        itemView.setOnClickListener {
            nimbusBranchesDelegate.onBranchItemClicked(branch)
        }
    }
}
