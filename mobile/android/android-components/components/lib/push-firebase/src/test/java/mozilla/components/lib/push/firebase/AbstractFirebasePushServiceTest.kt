/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.lib.push.firebase

import com.google.firebase.messaging.RemoteMessage
import mozilla.components.concept.push.EncryptedPushMessage
import mozilla.components.concept.push.PushError
import mozilla.components.concept.push.PushProcessor
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.`when`
import org.mockito.Mockito.reset
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyZeroInteractions

class AbstractFirebasePushServiceTest {

    private val processor: PushProcessor = mock()
    private val service = TestService()

    @Before
    fun setup() {
        reset(processor)
        PushProcessor.install(processor)
    }

    @Test
    fun `onNewToken passes token to processor`() {
        service.onNewToken("token")

        verify(processor).onNewToken("token")
    }

    @Test
    fun `new encrypted messages are passed to the processor`() {
        val remoteMessage: RemoteMessage = mock()
        val data = mapOf(
            "chid" to "1234",
            "body" to "contents",
            "con" to "encoding",
            "enc" to "salt",
            "cryptokey" to "dh256"
        )
        val captor = argumentCaptor<EncryptedPushMessage>()
        `when`(remoteMessage.data).thenReturn(data)
        service.onMessageReceived(remoteMessage)

        verify(processor).onMessageReceived(captor.capture())

        assertEquals("1234", captor.value.channelId)
        assertEquals("contents", captor.value.body)
        assertEquals("encoding", captor.value.encoding)
        assertEquals("salt", captor.value.salt)
        assertEquals("dh256", captor.value.cryptoKey)
    }

    @Test
    fun `malformed message exception handled with the processor`() {
        val remoteMessage: RemoteMessage = mock()
        val data = mapOf(
            "body" to "contents"
        )
        val captor = argumentCaptor<PushError>()
        `when`(remoteMessage.data).thenReturn(data)
        service.onMessageReceived(remoteMessage)

        verify(processor).onError(captor.capture())

        assertTrue(captor.value is PushError.MalformedMessage)
        assertTrue(captor.value.desc.contains("NoSuchElementException"))
    }

    @Test
    fun `processor is not called when no remote message provided`() {
        service.onMessageReceived(null)

        verifyZeroInteractions(processor)
    }

    class TestService : AbstractFirebasePushService()
}