/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.experiments

import android.annotation.SuppressLint
import android.content.Context
import android.os.Looper
import androidx.annotation.VisibleForTesting
import android.text.TextUtils
import mozilla.components.service.experiments.util.VersionString
import mozilla.components.support.base.log.logger.Logger
import java.util.zip.CRC32

/**
 * Class used to determine if a specific experiment should be enabled or not
 * for the device the app is running in
 *
 * @property valuesProvider provider for the device's values
 */
@Suppress("TooManyFunctions")
internal class ExperimentEvaluator(
    private val valuesProvider: ValuesProvider = ValuesProvider()
) {
    private val logger: Logger = Logger(LOG_TAG)

    internal fun findActiveExperiment(context: Context, experiments: List<Experiment>): ActiveExperiment? {
        for (experiment in experiments) {
            evaluate(context, ExperimentDescriptor(experiment.id), experiments)?.let {
                return it
            }
        }

        return null
    }

    /**
     * Determines if a specific experiment should be enabled or not for the device
     *
     * @param context context
     * @param experimentDescriptor experiment descriptor
     * @param experiments list of all experiments
     * @param userBucket device bucket
     *
     * @return experiment object if the device is part of the experiment, null otherwise
     */
    internal fun evaluate(
        context: Context,
        experimentDescriptor: ExperimentDescriptor,
        experiments: List<Experiment>,
        userBucket: Int = getUserBucket(context)
    ): ActiveExperiment? {
        val experiment = getExperiment(experimentDescriptor, experiments) ?: return null
        val isEnabled = isInBucket(userBucket, experiment) && matches(context, experiment)

        // If we have an active override for this experiment, return it.
        getOverride(context, experimentDescriptor, isEnabled)?.let {
            return if (!it.isEnabled) null else ActiveExperiment(experiment, it.branchName)
        }

        return if (isEnabled) {
            val rng = { a: Int, b: Int ->
                valuesProvider.getRandomBranchValue(a, b) }
            ActiveExperiment(
                experiment,
                pickActiveBranch(experiment, rng).name
            )
        } else {
            null
        }
    }

    /**
     * Finds an experiment given its descriptor
     *
     * @param descriptor experiment descriptor
     * @param experiments experiment list
     *
     * @return found experiment or null
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun getExperiment(descriptor: ExperimentDescriptor, experiments: List<Experiment>): Experiment? {
        return experiments.firstOrNull { it.id == descriptor.id }
    }

    private fun matches(context: Context, experiment: Experiment): Boolean {
        val match = experiment.match
        val region = valuesProvider.getRegion(context)
        val version = valuesProvider.getVersion(context) ?: ""
        val matchesRegion = (region == null) ||
            match.regions.isNullOrEmpty() ||
            match.regions.any { it == region }
        val tag = valuesProvider.getDebugTag()
        val matchesTag = (tag == null) ||
            match.debugTags.isNullOrEmpty() ||
            match.debugTags.any { it == tag }
        val matchesMinVersion = match.appMinVersion.isNullOrEmpty() ||
            VersionString(match.appMinVersion) <= VersionString(version)
        val matchesMaxVersion = match.appMaxVersion.isNullOrEmpty() ||
            VersionString(match.appMaxVersion) >= VersionString(version)

        return matchesRegion && matchesTag && matchesMinVersion && matchesMaxVersion &&
            matchesExperiment(match.appId, valuesProvider.getAppId(context)) &&
            matchesExperiment(match.appDisplayVersion, version) &&
            matchesExperiment(match.localeLanguage, valuesProvider.getLanguage(context)) &&
            matchesExperiment(match.localeCountry, valuesProvider.getCountry(context)) &&
            matchesExperiment(match.deviceManufacturer, valuesProvider.getManufacturer(context)) &&
            matchesExperiment(match.deviceModel, valuesProvider.getDevice(context))
    }

    private fun matchesExperiment(experimentValue: String?, deviceValue: String?): Boolean {
        return !(experimentValue != null &&
            !TextUtils.isEmpty(experimentValue) &&
            !(deviceValue?.matches(experimentValue.toRegex()) ?: false))
    }

    private fun isInBucket(userBucket: Int, experiment: Experiment): Boolean {
        if ((experiment.buckets.start + experiment.buckets.count) >= MAX_USER_BUCKET) {
            logger.warn("Experiment bucket is outside of the bucket range.")
        }
        return (userBucket >= experiment.buckets.start &&
                userBucket < (experiment.buckets.start + experiment.buckets.count))
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun getUserBucket(context: Context): Int {
        val uuid = valuesProvider.getClientId(context)
        val crc = CRC32()
        crc.update(uuid.toByteArray())
        val checksum = crc.value
        return (checksum % MAX_USER_BUCKET).toInt()
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun pickActiveBranch(
        experiment: Experiment,
        randomNumberGenerator: (Int, Int) -> Int
    ): Experiment.Branch {
        val sum = experiment.branches.sumBy { it.ratio }
        val diceRoll = randomNumberGenerator(0, sum)
        return selectBranchByWeight(diceRoll, experiment.branches)
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun selectBranchByWeight(
        diceRoll: Int,
        branches: List<Experiment.Branch>
    ): Experiment.Branch {
        assert(diceRoll >= 0) { "Dice roll should be >= 0." }
        assert(branches.isNotEmpty()) { "Branches list should not be empty." }

        var value = diceRoll
        for (branch in branches) {
            if (value < branch.ratio) {
                return branch
            }
            value -= branch.ratio
        }

        assert(false) { "Should have found matching branch." }
        return branches.last()
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal data class ExperimentOverride(
        val isEnabled: Boolean,
        val branchName: String
    )

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun getOverride(
        context: Context,
        descriptor: ExperimentDescriptor,
        isEnabledDefault: Boolean
    ): ExperimentOverride? {
        val prefEnabled = getSharedPreferencesEnabled(context)
        val prefBranch = getSharedPreferencesBranch(context)
        if (!prefEnabled.contains(descriptor.id) || !prefBranch.contains(descriptor.id)) {
            return null
        }

        val branchName = prefBranch.getString(descriptor.id, null) ?: return null

        return ExperimentOverride(
            prefEnabled.getBoolean(descriptor.id, isEnabledDefault),
            branchName
        )
    }

    /**
     * Overrides a specified experiment asynchronously
     *
     * @param context context
     * @param descriptor descriptor of the experiment
     * @param active overridden value for the experiment, true to activate it, false to deactivate
     * @param branchName overridden branch name for the experiment.
     */
    internal fun setOverride(
        context: Context,
        descriptor: ExperimentDescriptor,
        active: Boolean,
        branchName: String
    ) {
        getSharedPreferencesEnabled(context)
            .edit()
            .putBoolean(descriptor.id, active)
            .apply()
        getSharedPreferencesBranch(context)
            .edit()
            .putString(descriptor.id, branchName)
            .apply()
    }

    /**
     * Overrides a specified experiment as a blocking operation
     *
     * @param context context
     * @param descriptor descriptor of the experiment
     * @param active overridden value for the experiment, true to activate it, false to deactivate
     * @param branchName overridden branch name for the experiment.
     */
    @SuppressLint("ApplySharedPref")
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun setOverrideNow(
        context: Context,
        descriptor: ExperimentDescriptor,
        active: Boolean,
        branchName: String
    ) {
        require(Looper.myLooper() != Looper.getMainLooper()) { "This cannot be used on the main thread" }
        setOverride(context, descriptor, active, branchName)
    }

    /**
     * Clears an override for a specified experiment asynchronously
     *
     * @param context context
     * @param descriptor descriptor of the experiment
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun clearOverride(context: Context, descriptor: ExperimentDescriptor) {
        getSharedPreferencesEnabled(context)
            .edit()
            .remove(descriptor.id)
            .apply()
        getSharedPreferencesBranch(context)
            .edit()
            .remove(descriptor.id)
            .apply()
    }

    /**
     * Clears an override for a specified experiment as a blocking operation
     *
     * @param context context
     * @param descriptor descriptor of the experiment
     */
    @SuppressLint("ApplySharedPref")
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun clearOverrideNow(context: Context, descriptor: ExperimentDescriptor) {
        require(Looper.myLooper() != Looper.getMainLooper()) { "This cannot be used on the main thread" }
        clearOverride(context, descriptor)
    }

    /**
     * Clears all experiment overrides asynchronously
     *
     * @param context context
     */
    internal fun clearAllOverrides(context: Context) {
        getSharedPreferencesEnabled(context)
            .edit()
            .clear()
            .apply()
        getSharedPreferencesBranch(context)
            .edit()
            .clear()
            .apply()
    }

    /**
     * Clears all experiment overrides as a blocking operation
     *
     * @param context context
     */
    @SuppressLint("ApplySharedPref")
    internal fun clearAllOverridesNow(context: Context) {
        require(Looper.myLooper() != Looper.getMainLooper()) { "This cannot be used on the main thread" }
        clearAllOverrides(context)
    }

    private fun getSharedPreferencesBranch(context: Context) =
        context.getSharedPreferences(PREF_NAME_OVERRIDES_BRANCH, Context.MODE_PRIVATE)

    private fun getSharedPreferencesEnabled(context: Context) =
        context.getSharedPreferences(PREF_NAME_OVERRIDES_ENABLED, Context.MODE_PRIVATE)

    companion object {
        private const val LOG_TAG = "experiments"

        const val MAX_USER_BUCKET = 1000
        // This stores a boolean; whether an experiment is active or not.
        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        const val PREF_NAME_OVERRIDES_ENABLED = "mozilla.components.service.experiments.overrides.enabled"
        // This stores a string; the active branch name for the experiment.
        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        const val PREF_NAME_OVERRIDES_BRANCH = "mozilla.components.service.experiments.overrides.branch"
    }
}
