/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.push.firebase

import com.google.android.gms.common.ConnectionResult
import com.google.android.gms.common.GoogleApiAvailability
import com.google.firebase.messaging.RemoteMessage
import kotlinx.coroutines.Dispatchers
import mozilla.components.concept.push.EncryptedPushMessage
import mozilla.components.concept.push.PushProcessor
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.reset
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoInteractions
import org.mockito.Mockito.`when`
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.shadows.gms.Shadows
import org.robolectric.shadows.gms.common.ShadowGoogleApiAvailability

@RunWith(RobolectricTestRunner::class)
@Config(shadows = [ShadowGoogleApiAvailability::class])
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
            "cryptokey" to "dh256",
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
    fun `malformed message exception should not be thrown`() {
        val remoteMessage: RemoteMessage = mock()
        val data = mapOf(
            "chid" to "1234",
        )
        val captor = argumentCaptor<EncryptedPushMessage>()
        `when`(remoteMessage.data).thenReturn(data)
        service.onMessageReceived(remoteMessage)

        verify(processor, never()).onError(any())
        verify(processor).onMessageReceived(captor.capture())

        assertEquals("1234", captor.value.channelId)
        assertEquals("aes128gcm", captor.value.encoding)
        assertEquals("", captor.value.salt)
        assertEquals("", captor.value.cryptoKey)
    }

    @Test
    fun `do nothing if the message is not for us`() {
        val remoteMessage: RemoteMessage = mock()
        val data = mapOf(
            "con" to "encoding",
            "enc" to "salt",
            "cryptokey" to "dh256",
        )
        `when`(remoteMessage.data).thenReturn(data)

        service.onMessageReceived(remoteMessage)

        verifyNoInteractions(processor)
    }

    @Test
    fun `force registration should never be on Main`() {
        // Default dispatcher isn't main
        assertTrue(service.coroutineContext != Dispatchers.Main)

        val service = object : AbstractFirebasePushService(Dispatchers.Default) {}
        service.deleteToken()
    }

    @Test
    fun `service available reflects Google Play Services' availability`() {
        val service = TestService()

        // By default, service is unavailable.
        assertFalse(service.isServiceAvailable(testContext))

        val shadowGoogleApiAvailability = Shadows.shadowOf(GoogleApiAvailability.getInstance())
        shadowGoogleApiAvailability.setIsGooglePlayServicesAvailable(ConnectionResult.SUCCESS)

        assertTrue(service.isServiceAvailable(testContext))
    }

    class TestService : AbstractFirebasePushService()
}
