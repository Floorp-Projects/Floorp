/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.experiment

import mozilla.components.browser.engine.gecko.GeckoNimbus
import mozilla.components.support.base.log.logger.Logger
import org.json.JSONObject
import org.mozilla.experiments.nimbus.internal.FeatureHolder
import org.mozilla.geckoview.ExperimentDelegate
import org.mozilla.geckoview.ExperimentDelegate.ExperimentException
import org.mozilla.geckoview.ExperimentDelegate.ExperimentException.ERROR_FEATURE_NOT_FOUND
import org.mozilla.geckoview.GeckoResult

/**
 * Default Nimbus [ExperimentDelegate] implementation to communicate with mobile Gecko and GeckoView.
 */
class NimbusExperimentDelegate : ExperimentDelegate {

    private val logger = Logger(NimbusExperimentDelegate::javaClass.name)

    /**
     * Retrieves experiment information on the feature for use in GeckoView.
     *
     * @param feature Nimbus feature to retrieve information about
     * @return a [GeckoResult] with a JSON object containing experiment information or completes exceptionally.
     */
    override fun onGetExperimentFeature(feature: String): GeckoResult<JSONObject> {
        val result = GeckoResult<JSONObject>()
        val nimbusFeature = GeckoNimbus.getFeature(feature)
        if (nimbusFeature != null) {
            result.complete(nimbusFeature.toJSONObject())
        } else {
            logger.warn("Could not find Nimbus feature '$feature' to retrieve experiment information.")
            result.completeExceptionally(ExperimentException(ERROR_FEATURE_NOT_FOUND))
        }
        return result
    }

    /**
     * Records that an exposure event occurred with the feature.
     *
     * @param feature Nimbus feature to record information about
     * @return a [GeckoResult] that completes if the feature was found and recorded or completes exceptionally.
     */
    override fun onRecordExposureEvent(feature: String): GeckoResult<Void> {
        return recordWithFeature(feature) { it.recordExposure() }
    }

    /**
     * Records that an exposure event occurred with the feature, in a given experiment.
     * Note: See [onRecordExposureEvent] if no slug is known or needed
     *
     * @param feature Nimbus feature to record information about
     * @param slug Nimbus experiment slug to record information about
     * @return a [GeckoResult] that completes if the feature was found and recorded or completes exceptionally.
     */
    override fun onRecordExperimentExposureEvent(feature: String, slug: String): GeckoResult<Void> {
        return recordWithFeature(feature) { it.recordExperimentExposure(slug) }
    }

    /**
     * Records a malformed exposure event for the feature.
     *
     * @param feature Nimbus feature to record information about
     * @param part an optional detail or part identifier for then event. May be an empty string.
     * @return a [GeckoResult] that completes if the feature was found and recorded or completes exceptionally.
     */
    override fun onRecordMalformedConfigurationEvent(feature: String, part: String): GeckoResult<Void> {
        return recordWithFeature(feature) { it.recordMalformedConfiguration(part) }
    }

    /**
     * Convenience method to record experiment events and return the correct errors.
     *
     * @param featureId Nimbus feature to record information on
     * @param closure Nimbus record function to use
     * @return a [GeckoResult] that completes if successful or else with an exception
     */
    private fun recordWithFeature(featureId: String, closure: (FeatureHolder<*>) -> Unit): GeckoResult<Void> {
        val result = GeckoResult<Void>()
        val nimbusFeature = GeckoNimbus.getFeature(featureId)
        if (nimbusFeature != null) {
            closure(nimbusFeature)
            result.complete(null)
        } else {
            logger.warn("Could not find Nimbus feature '$featureId' to record an exposure event.")
            result.completeExceptionally(ExperimentException(ERROR_FEATURE_NOT_FOUND))
        }
        return result
    }
}
