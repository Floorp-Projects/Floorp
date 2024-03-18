/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.nimbus.ui

import android.view.View
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.experiments.nimbus.AvailableExperiment

/**
 * A view holder for displaying Nimbus experiment items.
 */
class NimbusExperimentItemViewHolder(
    view: View,
    private val nimbusExperimentsDelegate: NimbusExperimentsAdapterDelegate,
    private val titleView: TextView,
    private val summaryView: TextView,
) : RecyclerView.ViewHolder(view) {

    internal fun bind(experiment: AvailableExperiment) {
        titleView.text = experiment.userFacingName
        summaryView.text = experiment.userFacingDescription

        itemView.setOnClickListener {
            nimbusExperimentsDelegate.onExperimentItemClicked(experiment)
        }
    }
}
