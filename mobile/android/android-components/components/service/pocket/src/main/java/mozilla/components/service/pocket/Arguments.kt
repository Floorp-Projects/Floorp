/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket

/**
 * A collection of shared argument validation functions.
 */
internal object Arguments {

    fun assertIsValidUserAgent(userAgent: String) {
        assertIsNotBlank(userAgent, "user agent")
    }

    fun assertIsNotBlank(input: String, exceptionIdentifier: String) {
        require(input.isNotBlank()) { "Expected non-blank $exceptionIdentifier" }
    }
}
