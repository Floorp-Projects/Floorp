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
import org.mozilla.experiments.nimbus.EnrolledExperiment

/**
 * An adapter for displaying nimbus experiment items.
 */
class NimbusExperimentAdapter(
    private val nimbusExperimentsDelegate: NimbusExperimentsAdapterDelegate,
    experiments: List<EnrolledExperiment>
) : ListAdapter<EnrolledExperiment, NimbusExperimentItemViewHolder>(DiffCallback) {

    init {
        submitList(experiments)
    }

    override fun onCreateViewHolder(
        parent: ViewGroup,
        viewType: Int
    ): NimbusExperimentItemViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(R.layout.mozac_service_nimbus_experiment_item, parent, false)
        val titleView: TextView = view.findViewById(R.id.nimbus_experiment_name)
        val summaryView: TextView = view.findViewById(R.id.nimbus_experiment_description)
        return NimbusExperimentItemViewHolder(view, nimbusExperimentsDelegate, titleView, summaryView)
    }

    override fun onBindViewHolder(holder: NimbusExperimentItemViewHolder, position: Int) {
        holder.bind(getItem(position))
    }

    private object DiffCallback : DiffUtil.ItemCallback<EnrolledExperiment>() {
        override fun areContentsTheSame(oldItem: EnrolledExperiment, newItem: EnrolledExperiment) =
            oldItem.slug == newItem.slug

        override fun areItemsTheSame(oldItem: EnrolledExperiment, newItem: EnrolledExperiment) =
            oldItem.slug == newItem.slug
    }
}
