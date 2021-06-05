/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import mozilla.components.service.nimbus.NimbusApi
import mozilla.components.support.base.log.logger.Logger
import org.mozilla.experiments.nimbus.Variables
import org.mozilla.fenix.experiments.FeatureId

/**
 * Gets the branch name of an experiment acting on the feature given `featureId`, and transforms it
 * with given closure.
 *
 * You are probably looking for `withVariables`.
 *
 * If we're enrolled in an experiment, the transform is passed the branch id/slug as a `String`.
 *
 * If we're not enrolled in the experiment, or the experiment is not valid then the transform
 * is passed a `null`.
 */
fun <T> NimbusApi.withExperiment(featureId: FeatureId, transform: (String?) -> T): T {
    return transform(withExperiment(featureId))
}

/**
 * The synonym for [getExperimentBranch] to complement [withExperiment(String, (String?) -> T))].
 *
 * Short-hand for ` org.mozilla.experiments.nimbus.NimbusApi.getExperimentBranch`.
 */
@Suppress("TooGenericExceptionCaught")
fun NimbusApi.withExperiment(featureId: FeatureId) =
    try {
        getExperimentBranch(featureId.jsonName)
    } catch (e: Throwable) {
        Logger.error("Failed to getExperimentBranch(${featureId.jsonName})", e)
        null
    }

/**
 * Get the variables needed to configure the feature given by `featureId`.
 *
 * @param featureId The feature id that identifies the feature under experiment.
 *
 * @param sendExposureEvent Passing `true` to this parameter will record the exposure event
 *      automatically if the client is enrolled in an experiment for the given [featureId].
 *      Passing `false` here indicates that the application will manually record the exposure
 *      event by calling the `sendExposureEvent` function at the time of the exposure to the
 *      feature.
 *
 * See [sendExposureEvent] for more information on manually recording the event.
 *
 * @return a [Variables] object used to configure the feature.
 */
fun NimbusApi.getVariables(featureId: FeatureId, sendExposureEvent: Boolean = true) =
    getVariables(featureId.jsonName, sendExposureEvent)

/**
 * A synonym for `getVariables(featureId, sendExposureEvent)`.
 *
 * This exists as a complement to the `withVariable(featureId, sendExposureEvent, transform)` method.
 *
 * @param featureId the id of the feature as it appears in `Experimenter`
 * @param sendExposureEvent by default `true`. This logs an event that the user was exposed to an experiment
 *      involving this feature.
 * @return a `Variables` object providing typed accessors to a remotely configured JSON object.
 */
fun NimbusApi.withVariables(featureId: FeatureId, sendExposureEvent: Boolean = true) =
    getVariables(featureId, sendExposureEvent)

/**
 * Get a `Variables` object for this feature and use that to configure the feature itself or a
 * more type safe configuration object.
 *
 * @param featureId the id of the feature as it appears in `Experimenter`
 * @param sendExposureEvent by default `true`. This logs an event that the user was exposed to an experiment
 *      involving this feature.
 */
fun <T> NimbusApi.withVariables(featureId: FeatureId, sendExposureEvent: Boolean = true, transform: (Variables) -> T) =
    transform(getVariables(featureId, sendExposureEvent))

/**
 * Records the `exposure` event in telemetry.
 *
 * This is a manual function to accomplish the same purpose as passing `true` as the
 * `sendExposureEvent` property of the `getVariables` function. It is intended to be used
 * when requesting feature variables must occur at a different time than the actual user's
 * exposure to the feature within the app.
 *
 * - Examples:
 *     - If the `Variables` are needed at a different time than when the exposure to the feature
 *         actually happens, such as constructing a menu happening at a different time than the
 *         user seeing the menu.
 *     - If `getVariables` is required to be called multiple times for the same feature and it is
 *         desired to only record the exposure once, such as if `getVariables` were called
 *         with every keystroke.
 *
 * In the case where the use of this function is required, then the `getVariables` function
 * should be called with `false` so that the exposure event is not recorded when the variables
 * are fetched.
 *
 * This function is safe to call even when there is no active experiment for the feature. The SDK
 * will ensure that an event is only recorded for active experiments.
 *
 *  @param featureId string representing the id of the feature for which to record the exposure
 *     event.
 */
fun NimbusApi.recordExposureEvent(featureId: FeatureId) =
    recordExposureEvent(featureId.jsonName)
