/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.experiments

/**
 * Represents an experiment sync result
 */
internal data class ExperimentsSnapshot(
    /**
     * Downloaded list of experiments
     */
    val experiments: List<Experiment>,
    /**
     * Last time experiments were modified on the server, as a UNIX timestamp
     */
    val lastModified: Long?
)
