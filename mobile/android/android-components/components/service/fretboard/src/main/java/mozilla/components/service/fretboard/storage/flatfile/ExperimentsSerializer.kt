/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard.storage.flatfile

import mozilla.components.service.fretboard.Experiment
import mozilla.components.service.fretboard.JSONExperimentParser
import org.json.JSONArray
import org.json.JSONObject

/**
 * Helper class for serializing experiments
 * into the json format used to save them
 * locally on the device
 */
internal class ExperimentsSerializer {
    /**
     * Transforms the given list of experiments to
     * its json file representation
     *
     * @param experiments experiments to serialize
     * @return json file representation of the given experiments
     */
    fun toJson(experiments: List<Experiment>): String {
        val experimentsJson = JSONObject()
        val experimentsJSONArray = JSONArray()
        val jsonParser = JSONExperimentParser()
        for (experiment in experiments)
            experimentsJSONArray.put(jsonParser.toJson(experiment))
        experimentsJson.put(EXPERIMENTS_KEY, experimentsJSONArray)
        return experimentsJson.toString()
    }

    /**
     * Creates a list of experiments given json
     * representation, as stored locally inside a file
     *
     * @param json json file contents
     * @return list of experiments
     */
    fun fromJson(json: String): List<Experiment> {
        val experimentsJsonArray = JSONObject(json).getJSONArray(EXPERIMENTS_KEY)
        val experiments = ArrayList<Experiment>()
        val jsonParser = JSONExperimentParser()
        for (i in 0 until experimentsJsonArray.length())
            experiments.add(jsonParser.fromJson(experimentsJsonArray[i] as JSONObject))
        return experiments
    }

    companion object {
        private const val EXPERIMENTS_KEY = "experiments"
    }
}
