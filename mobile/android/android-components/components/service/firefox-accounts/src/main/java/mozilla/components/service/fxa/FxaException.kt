/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

/**
 * High-level exception class for the exceptions thrown in the Rust library.
 */
typealias FxaException = mozilla.appservices.fxaclient.FxaException

/**
 * Thrown on a network error.
 */
typealias FxaNetworkException = mozilla.appservices.fxaclient.FxaException.Network

/**
 * Thrown when the Rust library hits an assertion or panic (this is always a bug).
 */
typealias FxaPanicException = mozilla.appservices.fxaclient.FxaException.Panic

/**
 * Thrown when the operation requires additional authorization.
 */
typealias FxaUnauthorizedException = mozilla.appservices.fxaclient.FxaException.Unauthorized

/**
 * Thrown when the Rust library hits an unexpected error that isn't a panic.
 * This may indicate library misuse, network errors, etc.
 */
typealias FxaUnspecifiedException = mozilla.appservices.fxaclient.FxaException.Unspecified
