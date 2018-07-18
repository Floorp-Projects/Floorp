/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard

/**
 * Represents an experiment sync result
 */
data class ExperimentsSnapshot(
    val experiments: List<Experiment>,
    val lastModified: Long?
)
