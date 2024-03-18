/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.nimbus.ui

import android.view.LayoutInflater
import android.view.ViewGroup
import android.widget.ImageView
import android.widget.TextView
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.service.nimbus.R
import org.mozilla.experiments.nimbus.Branch

/**
 * An adapter for displaying a experimental branch for a Nimbus experiment.
 *
 * @param nimbusBranchesDelegate An instance of [NimbusBranchesAdapterDelegate] that provides
 * methods for handling the Nimbus branch items.
 */
class NimbusBranchAdapter(
    private val nimbusBranchesDelegate: NimbusBranchesAdapterDelegate,
) : RecyclerView.Adapter<RecyclerView.ViewHolder>() {

    // The list of [Branch]s to display.
    private var branches: List<Branch> = emptyList()

    // The selected [Branch] slug to highlight.
    private var selectedBranch: String = ""

    override fun onCreateViewHolder(
        parent: ViewGroup,
        viewType: Int,
    ): RecyclerView.ViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(R.layout.mozac_service_nimbus_branch_item, parent, false)
        val selectedIconView: ImageView = view.findViewById(R.id.selected_icon)
        val titleView: TextView = view.findViewById(R.id.nimbus_branch_name)
        val summaryView: TextView = view.findViewById(R.id.nimbus_branch_description)

        return NimbusBranchItemViewHolder(
            view,
            nimbusBranchesDelegate,
            selectedIconView,
            titleView,
            summaryView,
        )
    }

    override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
        holder as NimbusBranchItemViewHolder
        holder.bind(branches[position], selectedBranch)
    }

    override fun getItemCount(): Int = branches.size

    /**
     * Updates the list of [Branch]s and the selected branch that are displayed.
     *
     * @param branches The list of [Branch]s to display.
     * @param selectedBranch The [Branch] slug to highlight.
     */
    fun updateData(branches: List<Branch>, selectedBranch: String) {
        val diffUtil = DiffUtil.calculateDiff(
            NimbusBranchesDiffUtil(
                oldBranches = this.branches,
                newBranches = branches,
                oldSelectedBranch = this.selectedBranch,
                newSelectedBranch = selectedBranch,
            ),
        )

        this.branches = branches
        this.selectedBranch = selectedBranch

        diffUtil.dispatchUpdatesTo(this)
    }
}

internal class NimbusBranchesDiffUtil(
    private val oldBranches: List<Branch>,
    private val newBranches: List<Branch>,
    private val oldSelectedBranch: String,
    private val newSelectedBranch: String,
) : DiffUtil.Callback() {

    override fun areItemsTheSame(oldItemPosition: Int, newItemPosition: Int): Boolean =
        oldBranches[oldItemPosition].slug == newBranches[newItemPosition].slug &&
            oldSelectedBranch == newSelectedBranch

    override fun areContentsTheSame(oldItemPosition: Int, newItemPosition: Int): Boolean =
        oldBranches[oldItemPosition] == newBranches[newItemPosition]

    override fun getOldListSize(): Int = oldBranches.size

    override fun getNewListSize(): Int = newBranches.size
}
