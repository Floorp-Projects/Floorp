/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.experiments

/**
 * Helper function to create an [Experiment.Matcher] instance with default parameters.
 *
 * @return The matcher object.
 */
internal fun createDefaultMatcher(
    appId: String? = null,
    appDisplayVersion: String? = null,
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