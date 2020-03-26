/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.push.ext

import mozilla.components.feature.push.PushConnection
import mozilla.components.support.test.mock
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mockito.Mockito.`when`

class ConnectionKtTest {
    @Test
    fun `executes block with initialized connection`() {
        val connection: PushConnection = mock()
        var invoked = false

        connection.ifInitialized { invoked = true }

        assertFalse(invoked)

        `when`(connection.isInitialized()).thenReturn(true)

        connection.ifInitialized { invoked = true }

        assertTrue(invoked)
    }
}
