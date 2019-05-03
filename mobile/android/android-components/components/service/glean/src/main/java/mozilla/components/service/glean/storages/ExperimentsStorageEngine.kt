/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.storages

import android.annotation.SuppressLint
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.org.json.toJSON

import android.content.Context
import androidx.annotation.VisibleForTesting
import org.json.JSONObject

/**
 * This singleton handles the in-memory storage logic for keeping track of
 * the active experiments. It is meant to be used by through the methods in
 * the Glean class (General API).
 */
@SuppressLint("StaticFieldLeak")
internal object ExperimentsStorageEngine : StorageEngine {
    override lateinit var applicationContext: Context

    private val logger = Logger("glean/ExperimentsStorageEngine")

    // Maximum length of the experiment and branch names, in characters.
    private const val MAX_ID_LENGTH = 30

    private val experiments: MutableMap<String, RecordedExperimentData> = mutableMapOf()

    /**
     * Record an experiment as active.
     *
     * @param experimentId the id of the experiment
     * @param branch the active branch of the experiment
     * @param extra an optional, user defined String to String map used to provide richer experiment
     *              context if needed
     */
    fun setExperimentActive(
        experimentId: String,
        branch: String,
        extra: Map<String, String>? = null
    ) {
        val truncatedExperimentId = experimentId.let {
            if (it.length > MAX_ID_LENGTH) {
                logger.warn("experimentId ${it.length} > $MAX_ID_LENGTH")
                return@let it.substring(0, MAX_ID_LENGTH)
            }
            it
        }

        val truncatedBranch = branch.let {
            if (it.length > MAX_ID_LENGTH) {
                logger.warn("branch ${it.length} > $MAX_ID_LENGTH")
                return@let it.substring(0, MAX_ID_LENGTH)
            }
            it
        }

        val experimentData = RecordedExperimentData(truncatedBranch, extra)
        experiments.put(truncatedExperimentId, experimentData)
    }

    /**
     * Set an active experiment as inactive.
     *
     * Warns to the logger (but does not raise) if the experiment is not already active.
     *
     * @param experimentId the id of the experiment to set as inactive
     */
    fun setExperimentInactive(experimentId: String) {
        val truncatedExperimentId = experimentId.let {
            if (it.length > MAX_ID_LENGTH) {
                logger.warn("experimentId ${it.length} > $MAX_ID_LENGTH")
                return@let it.substring(0, MAX_ID_LENGTH)
            }
            it
        }

        if (!experiments.containsKey(truncatedExperimentId)) {
            logger.warn("experiment $experimentId} not already active")
        } else {
            experiments.remove(truncatedExperimentId)
        }
    }

    /**
     * Retrieves the data about active experiments
     *
     * @return the map of experiment ids to [RecordedExperimentData]
     */
    @Synchronized
    fun getSnapshot(): Map<String, RecordedExperimentData> {
        return experiments
    }

    /**
     * Get a snapshot of the stored data as a JSON object.
     *
     * @param storeName the name of the desired store.  Since experiments are
     *   included in all stores, this parameter is effectively ignored, but
     *   must be provided to comply with the StorageEngine interface.
     * @param clearStore whether or not to clearStore the requested store.
     *   This parameter is ignored.
     *
     * @return the JSONObject containing information about the active experiments
     */
    override fun getSnapshotAsJSON(
        @Suppress("UNUSED_PARAMETER") storeName: String,
        @Suppress("UNUSED_PARAMETER") clearStore: Boolean
    ): Any? {
        val pingExperiments = getSnapshot()

        if (pingExperiments.count() == 0) {
            return null
        }

        val experimentsMap = JSONObject()
        for ((key, value) in pingExperiments) {
            val experimentData = JSONObject()
            experimentData.put("branch", value.branch)
            if (value.extra != null) {
                experimentData.put("extra", value.extra.toJSON())
            }
            experimentsMap.put(key, experimentData)
        }
        return experimentsMap
    }

    override val sendAsTopLevelField: Boolean
        get() = true

    override fun clearAllStores() {
        experiments.clear()
    }
}

@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
data class RecordedExperimentData(
    val branch: String,
    val extra: Map<String, String>? = null
)
