/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

import mozilla.components.service.fxa.manager.GlobalAccountManager
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
suspend fun <T> handleFxaExceptions(
    logger: Logger,
    operation: String,
    block: suspend () -> T,
    postHandleAuthErrorBlock: (e: FxaUnauthorizedException) -> T,
    handleErrorBlock: (e: FxaException) -> T
): T {
    return try {
        logger.info("Executing: $operation")
        val res = block()
        logger.info("Successfully executed: $operation")
        res
    } catch (e: FxaException) {
        // We'd like to simply crash in case of certain errors (e.g. panics).
        if (shouldThrow(e)) {
            throw e
        }
        when (e) {
            is FxaUnauthorizedException -> {
                logger.warn("Auth error while running: $operation")
                GlobalAccountManager.authError(operation)
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
suspend fun <T> handleFxaExceptions(
    logger: Logger,
    operation: String,
    default: (error: FxaException) -> T,
    block: suspend () -> T
): T {
    return handleFxaExceptions(logger, operation, block, { default(it) }, { default(it) })
}

/**
 * Helper method that handles [FxaException] and returns a [Boolean] success flag as a result.
 */
suspend fun handleFxaExceptions(logger: Logger, operation: String, block: () -> Unit): Boolean {
    return handleFxaExceptions(logger, operation, { false }, {
        block()
        true
    })
}

private fun shouldThrow(e: FxaException): Boolean {
    return when (e) {
        // Throw on panics
        is FxaPanicException -> true
        // Don't throw for recoverable errors.
        is FxaNetworkException, is FxaUnauthorizedException, is FxaUnspecifiedException -> false
        // Throw on newly encountered exceptions.
        // If they're actually recoverable and you see them in crash reports, update this check.
        else -> true
    }
}
