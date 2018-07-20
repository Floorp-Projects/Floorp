/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fretboard

import android.content.Context
import android.text.TextUtils
import java.util.zip.CRC32

internal class ExperimentEvaluator(private val valuesProvider: ValuesProvider = ValuesProvider()) {
    fun evaluate(
        context: Context,
        experimentDescriptor: ExperimentDescriptor,
        experiments: List<Experiment>,
        userBucket: Int = getUserBucket(context)
    ): Experiment? {
        val experiment = getExperiment(experimentDescriptor, experiments) ?: return null
        val isEnabled = isInBucket(userBucket, experiment) && matches(context, experiment)
        context.getSharedPreferences(OVERRIDES_PREF_NAME, Context.MODE_PRIVATE).let {
            return if (it.getBoolean(experimentDescriptor.id, isEnabled)) experiment else null
        }
    }

    fun getExperiment(descriptor: ExperimentDescriptor, experiments: List<Experiment>): Experiment? {
        return experiments.firstOrNull { it.id == descriptor.id }
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
        return !(experiment.bucket?.min == null ||
            userBucket < experiment.bucket.min ||
            experiment.bucket.max == null ||
            userBucket >= experiment.bucket.max)
    }

    private fun getUserBucket(context: Context): Int {
        val uuid = DeviceUuidFactory(context).uuid
        val crc = CRC32()
        crc.update(uuid.toByteArray())
        val checksum = crc.value
        return (checksum % MAX_BUCKET).toInt()
    }

    fun setOverride(context: Context, descriptor: ExperimentDescriptor, active: Boolean) {
        context.getSharedPreferences(OVERRIDES_PREF_NAME, Context.MODE_PRIVATE)
            .edit()
            .putBoolean(descriptor.id, active)
            .apply()
    }

    fun clearOverride(context: Context, descriptor: ExperimentDescriptor) {
        context.getSharedPreferences(OVERRIDES_PREF_NAME, Context.MODE_PRIVATE)
            .edit()
            .remove(descriptor.id)
            .apply()
    }

    fun clearAllOverrides(context: Context) {
        context.getSharedPreferences(OVERRIDES_PREF_NAME, Context.MODE_PRIVATE)
            .edit()
            .clear()
            .apply()
    }

    companion object {
        private const val MAX_BUCKET = 100L
        private const val OVERRIDES_PREF_NAME = "mozilla.components.service.fretboard.overrides"
    }
}
