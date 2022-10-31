/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.push

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Test
import java.util.UUID

class EncryptedPushMessageTest {

    @Test
    fun `main constructor`() {
        val message = EncryptedPushMessage(UUID.randomUUID().toString(), "body", "encoding", "salt", "cryptoKey")

        assertNotNull(message.channelId)
        assertNotNull(message.body)
        assertNotNull(message.encoding)
        assertNotNull(message.salt)
        assertNotNull(message.cryptoKey)
    }

    @Test
    fun `main constructor with default parameters`() {
        val message = EncryptedPushMessage(UUID.randomUUID().toString(), "body", "encoding")

        assertNotNull(message.channelId)
        assertNotNull(message.body)
        assertNotNull(message.encoding)
        assertEquals("", message.salt)
        assertEquals("", message.cryptoKey)
    }

    @Test
    fun `invoke operator`() {
        val message = EncryptedPushMessage.invoke(UUID.randomUUID().toString(), "body", "encoding", "salt", "cryptoKey")

        assertNotNull(message.channelId)
        assertNotNull(message.body)
        assertNotNull(message.encoding)
        assertNotNull(message.salt)
        assertNotNull(message.cryptoKey)
    }

    @Test
    fun `invoke operator with default parameters`() {
        val message = EncryptedPushMessage.invoke(UUID.randomUUID().toString(), "body", "encoding")

        assertNotNull(message.channelId)
        assertNotNull(message.body)
        assertNotNull(message.encoding)
        assertNotNull("", message.salt)
        assertNotNull("", message.cryptoKey)
    }
}
