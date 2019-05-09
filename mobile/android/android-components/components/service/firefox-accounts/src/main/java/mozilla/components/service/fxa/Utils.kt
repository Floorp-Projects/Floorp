/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

/**
 * Runs a provided lambda, and if that throws non-panic FxA exception, runs the second lambda.
 *
 * @param block A lambda to execute which mail fail with an [FxaException].
 * @param handleErrorBlock A lambda to execute if [block] fails with a non-panic [FxaException].
 * @return object of type T, as defined by [block].
 */
fun <T> maybeExceptional(block: () -> T, handleErrorBlock: (e: FxaException) -> T): T {
    return try {
        block()
    } catch (e: FxaException) {
        when (e) {
            is FxaPanicException -> {
                throw e
            }
            else -> {
                handleErrorBlock(e)
            }
        }
    }
}
