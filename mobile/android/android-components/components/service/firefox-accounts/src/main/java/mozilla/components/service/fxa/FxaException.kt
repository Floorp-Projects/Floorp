/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

/**
 * High-level exception class for the exceptions thrown in the Rust library.
 */
typealias FxaException = org.mozilla.fxaclient.internal.FxaException

/**
 * Thrown on a network error.
 */
typealias FxaNetworkException = org.mozilla.fxaclient.internal.FxaException.Network

/**
 * Thrown when the Rust library hits an assertion or panic (this is always a bug).
 */
typealias FxaPanicException = org.mozilla.fxaclient.internal.FxaException.Panic

/**
 * Thrown when the operation requires additional authorization.
 */
typealias FxaUnauthorizedException = org.mozilla.fxaclient.internal.FxaException.Unauthorized

/**
 * Thrown when the Rust library hits an unexpected error that isn't a panic.
 * This may indicate library misuse, network errors, etc.
 */
typealias FxaUnspecifiedException = org.mozilla.fxaclient.internal.FxaException.Unspecified
