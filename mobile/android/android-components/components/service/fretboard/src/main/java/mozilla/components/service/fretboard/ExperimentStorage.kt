/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard

/**
 * Represents a location where experiments
 * are stored locally on the device
 */
interface ExperimentStorage {
    /**
     * Stores the given experiments to disk
     *
     * @param experiments list of experiments to store
     */
    fun save(snapshot: ExperimentsSnapshot)

    /**
     * Reads experiments from disk
     *
     * @return experiments from disk
     */
    fun retrieve(): ExperimentsSnapshot
}
