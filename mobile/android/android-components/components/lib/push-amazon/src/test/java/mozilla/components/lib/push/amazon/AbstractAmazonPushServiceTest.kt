/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.lib.push.amazon

import android.content.Context
import android.content.Intent
import android.os.Bundle
import com.amazon.device.messaging.ADM
import com.amazon.device.messaging.ADMMessageHandlerBase
import mozilla.components.concept.push.EncryptedPushMessage
import mozilla.components.concept.push.PushError
import mozilla.components.concept.push.PushProcessor
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.annotation.Implementation
import org.robolectric.annotation.Implements

@RunWith(RobolectricTestRunner::class)
@Config(shadows = [ShadowADMMessageHandlerBase::class, ShadowADM::class])
class AbstractAmazonPushServiceTest {
    private val processor: PushProcessor = mock()
    private val service = TestService()

    @Before
    fun setup() {
        Mockito.reset(processor)
        PushProcessor.install(processor)
    }

    @Test
    fun `onNewToken passes token to processor`() {
        service.onRegistered("token")

        Mockito.verify(processor).onNewToken("token")
    }

    @Test
    fun `new encrypted messages are passed to the processor`() {
        val messageIntent = Intent()
        val bundleExtra = Bundle()
        bundleExtra.putString("chid", "1234")
        bundleExtra.putString("body", "contents")
        bundleExtra.putString("con", "encoding")
        bundleExtra.putString("enc", "salt")
        bundleExtra.putString("cryptokey", "dh256")

        val captor = argumentCaptor<EncryptedPushMessage>()
        messageIntent.putExtras(bundleExtra)
        service.onMessage(messageIntent)

        Mockito.verify(processor).onMessageReceived(captor.capture())

        assertEquals("1234", captor.value.channelId)
        assertEquals("contents", captor.value.body)
        assertEquals("encoding", captor.value.encoding)
        assertEquals("salt", captor.value.salt)
        assertEquals("dh256", captor.value.cryptoKey)
    }

    @Test
    fun `registration errors are forwarded to the processor`() {
        val service = object: AbstractAmazonPushService() {
            public override fun onRegistrationError(errorId: String) {
                super.onRegistrationError(errorId)
            }
        }

        val captor = argumentCaptor<PushError>()

        service.onRegistrationError("123")

        Mockito.verify(processor).onError(captor.capture())

        assertTrue(captor.value is PushError.Registration)
        assertTrue(captor.value.desc.contains("registration failed"))
    }

    @Test
    fun `malformed message exception handled with the processor`() {
        val messageIntent = Intent()
        val bundleExtra = Bundle()
        bundleExtra.putString("body", "contents")

        val captor = argumentCaptor<PushError>()
        messageIntent.putExtras(bundleExtra)
        service.onMessage(messageIntent)

        Mockito.verify(processor).onError(captor.capture())

        assertTrue(captor.value is PushError.MalformedMessage)
        assertTrue(captor.value.desc.contains("NoSuchElementException"))
    }

    @Test
    fun `processor is not called when no intent message extras provided`() {
        val messageIntent = Intent()
        service.onMessage(messageIntent)

        Mockito.verifyZeroInteractions(processor)
    }

    @Test
    fun `service available reflects Amazon Device Messaging availability`() {
        assertTrue(service.isServiceAvailable(testContext))
    }

    class TestService : AbstractAmazonPushService()
}

/**
 * Custom Shadow for [ADMMessageHandlerBase].
 */
@Implements(ADMMessageHandlerBase::class)
class ShadowADMMessageHandlerBase {

    /**
     * [ADMMessageHandlerBase] has RuntimeException stub in the compileOnly constructor
     * which hinders unit testing. This overrides to exclude super() and the exception stub.
     */
    @Implementation
    @Suppress("UNUSED_PARAMETER")
    fun __constructor__(name: String) {}
}

/**
 * Custom Shadow for [ADM].
 */
@Implements(ADM::class)
class ShadowADM {

    @Implementation
    @Suppress("UNUSED_PARAMETER")
    fun __constructor__(context: Context) {
    }

    fun isSupported() = true
}
