/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.nimbus.ui

import android.view.LayoutInflater
import android.view.ViewGroup
import android.widget.TextView
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import mozilla.components.service.nimbus.R

/**
 * An adapter for displaying details related to a Nimbus experiment.
 */
class NimbusDetailAdapter(
    branches: List<String>
) : ListAdapter<String, NimbusDetailItemViewHolder>(DiffCallback) {

    init {
        submitList(branches)
    }

    override fun onCreateViewHolder(
        parent: ViewGroup,
        viewType: Int
    ): NimbusDetailItemViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(R.layout.mozac_service_nimbus_branch_item, parent, false)
        val titleView: TextView = view.findViewById(R.id.nimbus_branch_name)
        val summaryView: TextView = view.findViewById(R.id.nimbus_branch_description)

        return NimbusDetailItemViewHolder(view, titleView, summaryView)
    }

    override fun onBindViewHolder(holder: NimbusDetailItemViewHolder, position: Int) {
        holder.bind(getItem(position))
    }

    private object DiffCallback : DiffUtil.ItemCallback<String>() {
        override fun areContentsTheSame(oldItem: String, newItem: String) =
            oldItem == newItem

        override fun areItemsTheSame(oldItem: String, newItem: String) =
            oldItem == newItem
    }
}
