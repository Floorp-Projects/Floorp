/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard

/**
 * Entry point of the library
 *
 * @param source experiment remote source
 * @param storage experiment local storage mechanism
 */
class Fretboard(
    private val source: ExperimentSource,
    private val storage: ExperimentStorage
) {
    private var experiments: List<Experiment> = listOf()
    private var experimentsLoaded: Boolean = false

    /**
     * Loads experiments from local storage
     */
    fun loadExperiments() {
        experiments = storage.retrieve()
        experimentsLoaded = true
    }

    /**
     * Requests new experiments from the server and
     * saves them to local storage
     */
    fun updateExperiments() {
        if (!experimentsLoaded)
            loadExperiments()
        try {
            val serverExperiments = source.getExperiments(experiments)
            experiments = serverExperiments
            storage.save(serverExperiments)
        } catch (e: ExperimentDownloadException) {
            // Keep using the local experiments
        }
    }
}
