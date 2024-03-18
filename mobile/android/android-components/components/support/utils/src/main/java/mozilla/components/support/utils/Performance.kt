/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

import android.os.SystemClock
import mozilla.components.support.base.log.logger.Logger

/**
 * Executes the given [block] and logs the elapsed time in milliseconds.
 * Uses [System.nanoTime] for measurements, since it isn't tied to a wall-clock.
 *
 * @param logger [Logger] to use for logging.
 * @param op Name of the operation [block] performs.
 * @param block A lambda to measure.
 * @return [T] result of running [block].
 */
@SuppressWarnings("MagicNumber")
inline fun <T> logElapsedTime(logger: Logger, op: String, block: () -> T): T {
    logger.info("$op...")
    val start = SystemClock.elapsedRealtimeNanos()
    val res = block()
    logger.info("'$op' took ${(SystemClock.elapsedRealtimeNanos() - start) / 1_000_000} ms")
    return res
}
