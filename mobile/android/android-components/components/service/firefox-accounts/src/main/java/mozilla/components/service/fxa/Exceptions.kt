/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

/**
 * High-level exception class for the exceptions thrown in the Rust library.
 */
typealias FxaException = mozilla.appservices.fxaclient.FxaErrorException

/**
 * Thrown on a network error.
 */
typealias FxaNetworkException = mozilla.appservices.fxaclient.FxaErrorException.Network

/**
 * Thrown when the Rust library hits an assertion or panic (this is always a bug).
 */
typealias FxaPanicException = mozilla.appservices.fxaclient.FxaErrorException.Panic

/**
 * Thrown when the operation requires additional authorization.
 */
typealias FxaUnauthorizedException = mozilla.appservices.fxaclient.FxaErrorException.Authentication

/**
 * Thrown when the Rust library hits an unexpected error that isn't a panic.
 * This may indicate library misuse, network errors, etc.
 */
typealias FxaUnspecifiedException = mozilla.appservices.fxaclient.FxaErrorException.Other

/**
 * @return 'true' if this exception should be re-thrown and eventually crash the app.
 */
fun FxaException.shouldPropagate(): Boolean {
    return when (this) {
        // Throw on panics
        is FxaPanicException -> true
        // Don't throw for recoverable errors.
        is FxaNetworkException, is FxaUnauthorizedException, is FxaUnspecifiedException -> false
        // Throw on newly encountered exceptions.
        // If they're actually recoverable and you see them in crash reports, update this check.
        else -> true
    }
}

/**
 * Exceptions related to the account manager.
 */
sealed class AccountManagerException(message: String) : Exception(message) {
    /**
     * Hit a circuit-breaker during auth recovery flow.
     * @param operation An operation which triggered an auth recovery flow that hit a circuit breaker.
     */
    class AuthRecoveryCircuitBreakerException(operation: String) : AccountManagerException(
        "Auth recovery circuit breaker triggered by: $operation"
    )
}

/**
 * FxaException wrapper easily identifying it as the result of a failed operation of sending tabs.
 */
class SendCommandException(fxaException: FxaException) : Exception(fxaException)
