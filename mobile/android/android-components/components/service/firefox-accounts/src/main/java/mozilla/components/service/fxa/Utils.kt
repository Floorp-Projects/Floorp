/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

import mozilla.components.concept.sync.AuthFlowUrl
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.ServiceResult
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
    handleErrorBlock: (e: FxaException) -> T,
): T {
    return try {
        logger.info("Executing: $operation")
        val res = block()
        logger.info("Successfully executed: $operation")
        res
    } catch (e: FxaException) {
        // We'd like to simply crash in case of certain errors (e.g. panics).
        if (e.shouldPropagate()) {
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
    block: suspend () -> T,
): T {
    return handleFxaExceptions(logger, operation, block, { default(it) }, { default(it) })
}

/**
 * Helper method that handles [FxaException] and returns a [Boolean] success flag as a result.
 */
suspend fun handleFxaExceptions(logger: Logger, operation: String, block: () -> Unit): Boolean {
    return handleFxaExceptions(
        logger,
        operation,
        { false },
        {
            block()
            true
        },
    )
}

/**
 * Simplified version of Kotlin's inline class version that can be used as a return value.
 */
internal sealed class Result<out T> {
    data class Success<out T>(val value: T) : Result<T>()
    object Failure : Result<Nothing>()
}

/**
 * A helper function which allows retrying a [block] of suspend code for a few times in case it fails.
 *
 * @param logger [Logger] that will be used to log retry attempts/results
 * @param retryCount How many retry attempts are allowed
 * @param block A suspend function to execute
 * @return A [Result.Success] wrapping result of execution of [block] on (eventual) success,
 * or [Result.Failure] otherwise.
 */
internal suspend fun <T> withRetries(logger: Logger, retryCount: Int, block: suspend () -> T): Result<T> {
    var attempt = 0
    var res: T? = null
    while (attempt < retryCount && (res == null || res == false)) {
        attempt += 1
        logger.info("withRetries: attempt $attempt/$retryCount")
        res = block()
    }
    return if (res == null || res == false) {
        logger.warn("withRetries: all attempts failed")
        Result.Failure
    } else {
        Result.Success(res)
    }
}

/**
 * A helper function which allows retrying a [block] of suspend code for a few times in case it fails.
 * Short-circuits execution if [block] returns [ServiceResult.AuthError] during any of its attempts.
 *
 * @param logger [Logger] that will be used to log retry attempts/results
 * @param retryCount How many retry attempts are allowed
 * @param block A suspend function to execute
 * @return A [ServiceResult] representing result of [block] execution.
 */
internal suspend fun withServiceRetries(
    logger: Logger,
    retryCount: Int,
    block: suspend () -> ServiceResult,
): ServiceResult {
    var attempt = 0
    do {
        attempt += 1
        logger.info("withServiceRetries: attempt $attempt/$retryCount")
        when (val res = block()) {
            ServiceResult.Ok, ServiceResult.AuthError -> return res
            ServiceResult.OtherError -> {}
        }
    } while (attempt < retryCount)

    logger.warn("withServiceRetries: all attempts failed")
    return ServiceResult.OtherError
}

internal suspend fun String?.asAuthFlowUrl(account: OAuthAccount, scopes: Set<String>): AuthFlowUrl? {
    return if (this != null) {
        account.beginPairingFlow(this, scopes)
    } else {
        account.beginOAuthFlow(scopes)
    }
}
