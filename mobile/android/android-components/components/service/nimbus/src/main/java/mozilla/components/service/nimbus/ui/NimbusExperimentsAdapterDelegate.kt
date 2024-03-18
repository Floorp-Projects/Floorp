/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.nimbus.ui

import org.mozilla.experiments.nimbus.AvailableExperiment

/**
 * Provides methods for handling the experiment items in the Nimbus experiments manager.
 */
interface NimbusExperimentsAdapterDelegate {
    /**
     * Handler for when an experiment item is clicked.
     *
     * @param experiment The [AvailableExperiment] that was clicked.
     */
    fun onExperimentItemClicked(experiment: AvailableExperiment) = Unit
}
