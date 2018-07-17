/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard.source.kinto

import mozilla.components.service.fretboard.Experiment
import mozilla.components.service.fretboard.ExperimentSource
import mozilla.components.service.fretboard.JSONExperimentParser
import org.json.JSONArray
import org.json.JSONObject

/**
 * Class responsible for fetching and
 * parsing experiments from a Kinto server
 *
 * @param baseUrl Kinto server url
 * @param bucketName name of the bucket to fetch
 * @param collectionName name of the collection to fetch
 */
class KintoExperimentSource(
    val baseUrl: String,
    val bucketName: String,
    val collectionName: String,
    private val client: HttpClient = HttpURLConnectionHttpClient()
) : ExperimentSource {
    override fun getExperiments(experiments: List<Experiment>): List<Experiment> {
        val experimentsDiff = getExperimentsDiff(client, experiments)
        return mergeExperimentsFromDiff(experimentsDiff, experiments)
    }

    private fun getExperimentsDiff(client: HttpClient, experiments: List<Experiment>): String {
        val lastModified = getMaxLastModified(experiments)
        val kintoClient = KintoClient(client, baseUrl, bucketName, collectionName)
        return if (lastModified != null) {
            kintoClient.diff(lastModified)
        } else {
            kintoClient.get()
        }
    }

    private fun mergeExperimentsFromDiff(experimentsDiff: String, experiments: List<Experiment>): List<Experiment> {
        val mutableExperiments = experiments.toMutableList()
        val experimentParser = JSONExperimentParser()
        val diffJsonObject = JSONObject(experimentsDiff)
        val data = diffJsonObject.get(DATA_KEY)
        if (data is JSONObject) {
            if (data.getBoolean(DELETED_KEY)) {
                mergeDeleteDiff(data, mutableExperiments)
            }
        } else {
            mergeAddUpdateDiff(experimentParser, data as JSONArray, mutableExperiments)
        }
        return mutableExperiments
    }

    private fun mergeDeleteDiff(data: JSONObject, mutableExperiments: MutableList<Experiment>) {
        mutableExperiments.remove(mutableExperiments.single { it.id == data.getString(ID_KEY) })
    }

    private fun mergeAddUpdateDiff(
        experimentParser: JSONExperimentParser,
        experimentsJsonArray: JSONArray,
        mutableExperiments: MutableList<Experiment>
    ) {
        for (i in 0 until experimentsJsonArray.length()) {
            val experimentJsonObject = experimentsJsonArray[i] as JSONObject
            val experiment = mutableExperiments.singleOrNull { it.id == experimentJsonObject.getString(ID_KEY) }
            if (experiment != null)
                mutableExperiments.remove(experiment)
            mutableExperiments.add(experimentParser.fromJson(experimentJsonObject))
        }
    }

    private fun getMaxLastModified(experiments: List<Experiment>): Long? {
        var maxLastModified: Long = -1
        for (experiment in experiments) {
            val lastModified = experiment.lastModified
            if (lastModified != null && lastModified > maxLastModified) {
                maxLastModified = lastModified
            }
        }
        return if (maxLastModified > 0) maxLastModified else null
    }

    companion object {
        private const val ID_KEY = "id"
        private const val DATA_KEY = "data"
        private const val DELETED_KEY = "deleted"
    }
}
