/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard

/**
 * Represents a location where experiments are stored
 * (Kinto, a JSON file on a server, etc)
 */
interface ExperimentSource {
    /**
     * Requests new experiments from the source,
     * parsing the response into experiments
     *
     * @param client Http client to use, provided by Fretboard
     * @param experiments list of already downloaded experiments
     * (in order to process a diff response, for example)
     * @return modified list of experiments
     */
    fun getExperiments(experiments: List<Experiment>): List<Experiment>
}
