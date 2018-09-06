/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard

import android.annotation.SuppressLint
import android.content.Context
import android.os.Looper
import android.text.TextUtils
import java.util.zip.CRC32

/**
 * Class used to determine if a specific experiment should be enabled or not
 * for the device the app is running in
 *
 * @property valuesProvider provider for the device's values
 */
@Suppress("TooManyFunctions")
internal class ExperimentEvaluator(private val valuesProvider: ValuesProvider = ValuesProvider()) {
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
    fun evaluate(
        context: Context,
        experimentDescriptor: ExperimentDescriptor,
        experiments: List<Experiment>,
        userBucket: Int = getUserBucket(context)
    ): Experiment? {
        val experiment = getExperiment(experimentDescriptor, experiments) ?: return null
        val isEnabled = isInBucket(userBucket, experiment) && matches(context, experiment)
        context.getSharedPreferences(OVERRIDES_PREF_NAME, Context.MODE_PRIVATE).let {
            return if (it.getBoolean(experimentDescriptor.name, isEnabled)) experiment else null
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
    fun getExperiment(descriptor: ExperimentDescriptor, experiments: List<Experiment>): Experiment? {
        return experiments.firstOrNull { it.name == descriptor.name }
    }

    private fun matches(context: Context, experiment: Experiment): Boolean {
        if (experiment.match != null) {
            val region = valuesProvider.getRegion(context)
            val matchesRegion = !(region != null &&
                                        experiment.match.regions != null &&
                                        experiment.match.regions.isNotEmpty() &&
                                        experiment.match.regions.none { it == region })
            val releaseChannel = valuesProvider.getReleaseChannel(context)
            val matchesReleaseChannel = releaseChannel == null ||
                                                experiment.match.releaseChannel == null ||
                                                releaseChannel == experiment.match.releaseChannel
            return matchesRegion &&
                matchesReleaseChannel &&
                matchesExperiment(experiment.match.appId, valuesProvider.getAppId(context)) &&
                matchesExperiment(experiment.match.language, valuesProvider.getLanguage(context)) &&
                matchesExperiment(experiment.match.country, valuesProvider.getCountry(context)) &&
                matchesExperiment(experiment.match.version, valuesProvider.getVersion(context)) &&
                matchesExperiment(experiment.match.manufacturer, valuesProvider.getManufacturer(context)) &&
                matchesExperiment(experiment.match.device, valuesProvider.getDevice(context))
        }
        return true
    }

    private fun matchesExperiment(experimentValue: String?, deviceValue: String): Boolean {
        return !(experimentValue != null &&
            !TextUtils.isEmpty(experimentValue) &&
            !deviceValue.matches(experimentValue.toRegex()))
    }

    private fun isInBucket(userBucket: Int, experiment: Experiment): Boolean {
        return (experiment.bucket?.min == null ||
            userBucket >= experiment.bucket.min) &&
            (experiment.bucket?.max == null ||
            userBucket < experiment.bucket.max)
    }

    private fun getUserBucket(context: Context): Int {
        val uuid = DeviceUuidFactory(context).uuid
        val crc = CRC32()
        crc.update(uuid.toByteArray())
        val checksum = crc.value
        return (checksum % MAX_BUCKET).toInt()
    }

    /**
     * Overrides a specified experiment asynchronously
     *
     * @param context context
     * @param descriptor descriptor of the experiment
     * @param active overridden value for the experiment, true to activate it, false to deactivate
     */
    fun setOverride(context: Context, descriptor: ExperimentDescriptor, active: Boolean) {
        context.getSharedPreferences(OVERRIDES_PREF_NAME, Context.MODE_PRIVATE)
            .edit()
            .putBoolean(descriptor.name, active)
            .apply()
    }

    /**
     * Overrides a specified experiment as a blocking operation
     *
     * @param context context
     * @param descriptor descriptor of the experiment
     * @param active overridden value for the experiment, true to activate it, false to deactivate
     */
    @SuppressLint("ApplySharedPref")
    fun setOverrideNow(context: Context, descriptor: ExperimentDescriptor, active: Boolean) {
        require(Looper.myLooper() != Looper.getMainLooper()) { "This cannot be used on the main thread" }
        context.getSharedPreferences(OVERRIDES_PREF_NAME, Context.MODE_PRIVATE)
                .edit()
                .putBoolean(descriptor.name, active)
                .commit()
    }

    /**
     * Clears an override for a specified experiment asynchronously
     *
     * @param context context
     * @param descriptor descriptor of the experiment
     */
    fun clearOverride(context: Context, descriptor: ExperimentDescriptor) {
        context.getSharedPreferences(OVERRIDES_PREF_NAME, Context.MODE_PRIVATE)
            .edit()
            .remove(descriptor.name)
            .apply()
    }

    /**
     * Clears an override for a specified experiment as a blocking operation
     *
     * @param context context
     * @param descriptor descriptor of the experiment
     */
    @SuppressLint("ApplySharedPref")
    fun clearOverrideNow(context: Context, descriptor: ExperimentDescriptor) {
        require(Looper.myLooper() != Looper.getMainLooper()) { "This cannot be used on the main thread" }
        context.getSharedPreferences(OVERRIDES_PREF_NAME, Context.MODE_PRIVATE)
                .edit()
                .remove(descriptor.name)
                .commit()
    }

    /**
     * Clears all experiment overrides asynchronously
     *
     * @param context context
     */
    fun clearAllOverrides(context: Context) {
        context.getSharedPreferences(OVERRIDES_PREF_NAME, Context.MODE_PRIVATE)
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
    fun clearAllOverridesNow(context: Context) {
        require(Looper.myLooper() != Looper.getMainLooper()) { "This cannot be used on the main thread" }
        context.getSharedPreferences(OVERRIDES_PREF_NAME, Context.MODE_PRIVATE)
                .edit()
                .clear()
                .commit()
    }

    companion object {
        private const val MAX_BUCKET = 100L
        private const val OVERRIDES_PREF_NAME = "mozilla.components.service.fretboard.overrides"
    }
}
