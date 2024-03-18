/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.testing

/**
 * This implements a JUnit rule for writing tests for Glean SDK metrics.
 *
 * The rule takes care of sending Glean SDK pings to a local server, at the
 * address: "http://localhost:<port>".
 *
 * This is useful for Android instrumented tests, where we don't want to
 * initialize Glean more than once but still want to send pings to a local
 * server for validation.
 *
 * FIXME(bug 1787234): State of the local server can persist across multiple test classes,
 * leading to hard-to-diagnose intermittent test failures.
 * It might be necessary to limit use of `GleanTestLocalServer` to a single test class for now.
 *
 * Example usage:
 *
 * ```
 * // Add the following lines to you test class.
 * @get:Rule
 * val gleanRule = GleanTestLocalServer(3785)
 * ```
 *
 * @param localPort the port of the local ping server
 */
typealias GleanTestLocalServer = mozilla.telemetry.glean.testing.GleanTestLocalServer
