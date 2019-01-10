package mozilla.components.service.fxa

/**
 * Wrapper class for the exceptions thrown in the Rust library.
 *
 * Broken down into the following subclasses:
 *
 * - `FxaException.Unauthorized`: Thrown when the operation requires additional authorization.
 * - `FxaException.Network`: Thrown on a network error.
 * - `FxaException.Panic`: Thrown when the Rust library hits an assertion or panic (this is always
 *   a bug).
 * - `FxaException.Unspecified`: Thrown when the Rust library hits an unexpected error that isn't
 *   a panic. This may indicate library misuse, network errors, etc.
 *
 */
typealias FxaException = org.mozilla.fxaclient.internal.FxaException
