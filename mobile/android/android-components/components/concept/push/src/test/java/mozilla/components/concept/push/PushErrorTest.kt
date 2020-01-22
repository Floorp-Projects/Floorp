/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.push

import org.junit.Assert.assertEquals
import org.junit.Test

class PushErrorTest {
    @Test
    fun `all PushError sets description`() {
        // This test is mostly to satisfy coverage.

        var error: PushError = PushError.MalformedMessage("message")
        assertEquals("message", error.message)

        error = PushError.Network("network")
        assertEquals("network", error.message)

        error = PushError.Registration("reg")
        assertEquals("reg", error.message)

        val exception = IllegalStateException()
        val rustError = PushError.Rust(exception, "rust")
        assertEquals("rust", rustError.message)
        assertEquals(exception, rustError.cause)

        error = PushError.ServiceUnavailable("service")
        assertEquals("service", error.message)
    }
}
