/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

import mozilla.components.concept.sync.AuthException
import mozilla.components.concept.sync.AuthExceptionType
import mozilla.components.service.fxa.manager.authErrorRegistry
import mozilla.components.support.base.log.logger.Logger

/**
 * Runs a provided lambda, and if that throws non-panic, non-auth FxA exception, runs [handleErrorBlock].
 * If that lambda throws an FxA auth exception, notifies [authErrorRegistry], and runs [postHandleAuthErrorBlock].
 *
 * @param block A lambda to execute which mail fail with an [FxaException].
 * @param postHandleAuthErrorBlock A lambda to execute if [block] failed with [FxaUnauthorizedException].
 * @param handleErrorBlock A lambda to execute if [block] fails with a non-panic, non-auth [FxaException].
 * @return object of type T, as defined by [block].
 */
fun <T> handleFxaExceptions(
    logger: Logger,
    operation: String,
    block: () -> T,
    postHandleAuthErrorBlock: (e: FxaUnauthorizedException) -> T,
    handleErrorBlock: (e: FxaException) -> T
): T {
    return try {
        logger.info("Executing: $operation")
        val res = block()
        logger.info("Successfully executed: $operation")
        res
    } catch (e: FxaException) {
        when (e) {
            is FxaPanicException -> {
                throw e
            }
            is FxaUnauthorizedException -> {
                logger.warn("Auth error while running: $operation")
                authErrorRegistry.notifyObservers { onAuthErrorAsync(AuthException(AuthExceptionType.UNAUTHORIZED, e)) }
                postHandleAuthErrorBlock(e)
            }
            else -> {
                logger.error("Error while running: $operation", e)
                handleErrorBlock(e)
            }
        }
    }
}

/**
 * Helper method that handles [FxaException] and allows specifying a lazy default value via [default]
 * block for use in case of errors. Execution is wrapped in log statements.
 */
fun <T> handleFxaExceptions(logger: Logger, operation: String, default: (error: FxaException) -> T, block: () -> T): T {
    return handleFxaExceptions(logger, operation, block, { default(it) }, { default(it) })
}

/**
 * Helper method that handles [FxaException] and returns a [Boolean] success flag as a result.
 */
fun handleFxaExceptions(logger: Logger, operation: String, block: () -> Unit): Boolean {
    return handleFxaExceptions(logger, operation, { false }, {
        block()
        true
    })
}
