/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.experiments

import androidx.work.WorkInfo
import androidx.work.WorkManager
import java.util.concurrent.ExecutionException

/**
 * Helper function to check to see if a worker has been scheduled with the [WorkManager]
 *
 * @param tag a string representing the worker tag
 * @return True if the task found in [WorkManager], false otherwise
 */
internal fun isWorkScheduled(tag: String): Boolean {
    val instance = WorkManager.getInstance()
    val statuses = instance.getWorkInfosByTag(tag)
    try {
        val workInfoList = statuses.get()
        for (workInfo in workInfoList) {
            val state = workInfo.state
            if ((state === WorkInfo.State.RUNNING) || (state === WorkInfo.State.ENQUEUED)) {
                return true
            }
        }
    } catch (e: ExecutionException) {
        // Do nothing but will return false
    } catch (e: InterruptedException) {
        // Do nothing but will return false
    }

    return false
}

/**
 * Helper function to return [WorkInfo] from the associated tag in [WorkManager] or null if no work
 * exists for that tag.
 *
 * @param tag a string representing the worker tag
 * @return [WorkInfo] for the tag that was passed in or null
 */
internal fun getWorkInfoByTag(tag: String): WorkInfo? {
    val instance = WorkManager.getInstance()
    val statuses = instance.getWorkInfosByTag(tag)
    try {
        val workInfoList = statuses.get()
        return workInfoList.firstOrNull()
    } catch (e: ExecutionException) {
        // Do nothing but will return false
    } catch (e: InterruptedException) {
        // Do nothing but will return false
    }

    return null
}

/**
 * Helper function to create an [Experiment.Matcher] instance with default parameters.
 *
 * @return The matcher object.
 */
internal fun createDefaultMatcher(
    appId: String? = null,
    appDisplayVersion: String? = null,
    appMinVersion: String? = null,
    appMaxVersion: String? = null,
    localeLanguage: String? = null,
    localeCountry: String? = null,
    deviceManufacturer: String? = null,
    deviceModel: String? = null,
    regions: List<String>? = null,
    debugTags: List<String>? = null
): Experiment.Matcher {
    return Experiment.Matcher(
        appId,
        appDisplayVersion,
        appMinVersion,
        appMaxVersion,
        localeLanguage,
        localeCountry,
        deviceManufacturer,
        deviceModel,
        regions,
        debugTags
    )
}

/**
 * Helper function to easily create a branch list with just one branch for all participants.
 *
 * @return The branch list.
 */
internal fun createSingleBranchList(): List<Experiment.Branch> {
    return listOf(Experiment.Branch("only-branch", 1))
}

/**
 * Helper function to create an [Experiment] instance with default parameters.
 *
 * @return The experiment object.
 */
internal fun createDefaultExperiment(
    id: String = "testid",
    description: String = "description",
    lastModified: Long? = null,
    schemaModified: Long? = null,
    buckets: Experiment.Buckets = Experiment.Buckets(0, ExperimentEvaluator.MAX_USER_BUCKET),
    branches: List<Experiment.Branch> = createSingleBranchList(),
    match: Experiment.Matcher = createDefaultMatcher()
): Experiment {
    return Experiment(
        id,
        description,
        lastModified,
        schemaModified,
        buckets,
        branches,
        match
    )
}
