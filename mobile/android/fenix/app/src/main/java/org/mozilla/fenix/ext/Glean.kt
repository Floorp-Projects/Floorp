/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import mozilla.telemetry.glean.private.TimingDistributionMetricType

/**
 * A reimplementation of [TimingDistributionMetricType.measure] that address unintuitive
 * issues around non-local returns: see https://bugzilla.mozilla.org/show_bug.cgi?id=1699505.
 * This should be removed once that bug is resolved. That method's kdoc is as follows:
 *
 * Convenience method to simplify measuring a function or block of code.
 *
 * If the measured function throws, the measurement is canceled and the exception rethrown.
 */
@Suppress("TooGenericExceptionCaught")
fun <U> TimingDistributionMetricType.measureNoInline(funcToMeasure: () -> U): U {
    val timerId = start()

    val returnValue = try {
        funcToMeasure()
    } catch (e: Exception) {
        cancel(timerId)
        throw e
    }

    stopAndAccumulate(timerId)
    return returnValue
}
