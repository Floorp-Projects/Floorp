/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
@file:Suppress("MatchingDeclarationName")

package mozilla.components.service.glean.error

import androidx.annotation.VisibleForTesting
import mozilla.components.service.glean.Dispatchers
import mozilla.components.service.glean.private.CommonMetricData
import mozilla.components.service.glean.private.CounterMetricType
import mozilla.components.service.glean.private.Lifetime
import mozilla.components.service.glean.storages.CountersStorageEngine
import mozilla.components.support.base.log.logger.Logger

// The labeled counter metrics that store the errors are defined in the
// `metrics.yaml` for documentation purposes, but are not actually used
// directly, since the `sendInPings` value needs to match the pings of the
// metric that is erroring (plus the "metrics" ping), not some constant value
// that we could define in `metrics.yaml`.

object ErrorRecording {
    internal enum class ErrorType {
        InvalidValue,
        InvalidLabel
    }

    @Suppress("TopLevelPropertyNaming")
    internal val GLEAN_ERROR_NAMES = mapOf(
        ErrorType.InvalidValue to "invalid_value",
        ErrorType.InvalidLabel to "invalid_label"
    )

    /**
     * Record an error that will be sent as a labeled counter in the `glean.error` category
     * in pings.
     *
     * @param metricData The [CommonMetricData] instance associated with the error.
     * @param errorType The [ErrorType] type of error.
     * @param message The message to send to `logger.warn`.  This message is not sent with the
     *     ping.  It does not need to include the metric name, as that is automatically
     *     prepended to the message.
     * @param logger The [Logger] instance to display the warning.
     * @param numErrors The optional number of errors to report for this [ErrorType].
     */
    internal fun recordError(
        metricData: CommonMetricData,
        errorType: ErrorType,
        message: String,
        logger: Logger,
        numErrors: Int? = null
    ) {
        val errorName = GLEAN_ERROR_NAMES[errorType]!!

        val identifier = metricData.identifier.split("/", limit = 2)[0]

        // Record errors in the pings the metric is in, as well as the metrics ping.
        var sendInPings = metricData.sendInPings
        if (!sendInPings.contains("metrics")) {
            sendInPings = sendInPings + listOf("metrics")
        }

        val errorMetric = CounterMetricType(
            disabled = false,
            category = "glean.error",
            lifetime = Lifetime.Ping,
            name = "$errorName/$identifier",
            sendInPings = sendInPings
        )

        logger.warn("${metricData.identifier}: $message")

        // There are two reasons for using `CountersStorageEngine.record` below
        // and not just using the public `CounterMetricType` API.

        // 1) The labeled counter metrics that store the errors are defined in the
        // `metrics.yaml` for documentation purposes, but are not actually used
        // directly, since the `sendInPings` value needs to match the pings of the
        // metric that is erroring (plus the "metrics" ping), not some constant value
        // that we could define in `metrics.yaml`.

        // 2) We want to bybass the restriction that there are only N values in a
        // dynamically labeled metric.  Error reporting should never report errors
        // in the __other__ category.
        CountersStorageEngine.record(
            errorMetric,
            amount = numErrors ?: 1
        )
    }

    /**
    * Get the number of recorded errors for the given metric and error type.
    *
    * @param metricData The metric the errors were reported about
    * @param errorType The type of error
    * @param pingName The name of the ping (optional).  If null, will use
    *     metricData.sendInPings.first()
    * @return The number of errors reported
    */
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    internal fun testGetNumRecordedErrors(
        metricData: CommonMetricData,
        errorType: ErrorType,
        pingName: String? = null
    ): Int {
        @Suppress("EXPERIMENTAL_API_USAGE")
        Dispatchers.API.assertInTestingMode()

        val usePingName = pingName?.let {
            pingName
        } ?: run {
            metricData.sendInPings.first()
        }

        val errorName = GLEAN_ERROR_NAMES[errorType]!!

        val errorMetric = CounterMetricType(
            disabled = false,
            category = "glean.error",
            lifetime = Lifetime.Ping,
            name = "$errorName/${metricData.identifier}",
            sendInPings = metricData.sendInPings
        )

        return errorMetric.testGetValue(usePingName)
    }
}
