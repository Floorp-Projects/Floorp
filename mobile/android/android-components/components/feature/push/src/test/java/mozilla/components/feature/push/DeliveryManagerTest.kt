/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.push

import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mockito.Mockito.`when`

class DeliveryManagerTest {

    @Test
    fun `serviceType found for PushType`() {
        val pushType = DeliveryManager.serviceForChannelId("992a0f0542383f1ea5ef51b7cf4ae6c4")
        assertEquals(PushType.Services, pushType)
    }

    @Test(expected = NoSuchElementException::class)
    fun `exception thrown if serviceType not found`() {
        DeliveryManager.serviceForChannelId("992a0f0542383f1ea5ef51b7cf4ea6c4")
    }

    @Test
    fun `DeliveryManager executes block with initialized connection`() {
        val connection: PushConnection = mock()
        var invoked = false

        DeliveryManager.with(connection) { invoked = true }

        assertFalse(invoked)

        `when`(connection.isInitialized()).thenReturn(true)

        DeliveryManager.with(connection) { invoked = true }

        assertTrue(invoked)
    }
}
