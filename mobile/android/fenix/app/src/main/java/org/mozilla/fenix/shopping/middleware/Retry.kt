/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.middleware

import kotlinx.coroutines.delay

private const val MAX_RETRIES = 10
private const val INITIAL_DELAY_MS = 2000L
private const val MAX_DELAY_MS = 20000L
private const val FACTOR = 2.0

/**
 * Retry a suspend function until it returns a value that satisfies the predicate.
 *
 * @param maxRetries The maximum number of retries.
 * @param initialDelayMs The initial delay in milliseconds.
 * @param maxDelayMs The maximum delay in milliseconds.
 * @param factor The factor to increase the delay by.
 * @param predicate The predicate to check the result against.
 * @param block The function to retry.
 */
suspend fun <T> retry(
    maxRetries: Int = MAX_RETRIES,
    initialDelayMs: Long = INITIAL_DELAY_MS,
    maxDelayMs: Long = MAX_DELAY_MS,
    factor: Double = FACTOR,
    predicate: (T) -> Boolean,
    block: suspend () -> T,
): T {
    var delayTime = initialDelayMs
    var data: T = block()

    repeat(maxRetries - 1) {
        if (predicate(data)) {
            delay(delayTime)
            data = block()
            delayTime = (delayTime * factor).toLong().coerceAtMost(maxDelayMs)
        } else {
            return data
        }
    }
    return data
}
