/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.studies

import org.mozilla.experiments.nimbus.internal.EnrolledExperiment

/**
 * Provides methods for handling the studies items.
 */
interface StudiesAdapterDelegate {
    /**
     * Handler for when the remove button is clicked.
     *
     * @param experiment The [EnrolledExperiment] to remove.
     */
    fun onRemoveButtonClicked(experiment: EnrolledExperiment) = Unit
}
