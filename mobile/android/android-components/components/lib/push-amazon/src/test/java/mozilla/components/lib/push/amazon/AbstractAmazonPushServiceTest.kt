/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.push.amazon

import android.content.Context
import android.content.Intent
import android.os.Bundle
import com.amazon.device.messaging.ADM
import com.amazon.device.messaging.ADMMessageHandlerBase
import mozilla.components.concept.push.EncryptedPushMessage
import mozilla.components.concept.push.PushError
import mozilla.components.concept.push.PushProcessor
import mozilla.components.concept.push.PushService.Companion.MESSAGE_KEY_BODY
import mozilla.components.concept.push.PushService.Companion.MESSAGE_KEY_CHANNEL_ID
import mozilla.components.concept.push.PushService.Companion.MESSAGE_KEY_CRYPTO_KEY
import mozilla.components.concept.push.PushService.Companion.MESSAGE_KEY_ENCODING
import mozilla.components.concept.push.PushService.Companion.MESSAGE_KEY_SALT
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.anyString
import org.mockito.Mockito.never
import org.mockito.Mockito.reset
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoInteractions
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
        reset(processor)
        PushProcessor.install(processor)
    }

    @Test
    fun `if registrationId exists startRegister invokes onNewToken`() {
        val testService = TestService()

        testService.start(testContext)
        verify(processor).onNewToken(anyString())
    }

    @Test
    fun `onNewToken passes token to processor`() {
        service.onRegistered("token")

        verify(processor).onNewToken("token")
    }

    @Test
    fun `new encrypted messages are passed to the processor`() {
        val messageIntent = Intent()
        val bundleExtra = Bundle()
        bundleExtra.putString(MESSAGE_KEY_CHANNEL_ID, "1234")
        bundleExtra.putString(MESSAGE_KEY_BODY, "contents")
        bundleExtra.putString(MESSAGE_KEY_ENCODING, "encoding")
        bundleExtra.putString(MESSAGE_KEY_SALT, "salt")
        bundleExtra.putString(MESSAGE_KEY_CRYPTO_KEY, "dh256")

        val captor = argumentCaptor<EncryptedPushMessage>()
        messageIntent.putExtras(bundleExtra)
        service.onMessage(messageIntent)

        verify(processor).onMessageReceived(captor.capture())

        assertEquals("1234", captor.value.channelId)
        assertEquals("contents", captor.value.body)
        assertEquals("encoding", captor.value.encoding)
        assertEquals("salt", captor.value.salt)
        assertEquals("dh256", captor.value.cryptoKey)
    }

    @Test
    fun `registration errors are forwarded to the processor`() {
        val service = object : AbstractAmazonPushService() {
            public override fun onRegistrationError(errorId: String) {
                super.onRegistrationError(errorId)
            }
        }

        val captor = argumentCaptor<PushError>()

        service.onRegistrationError("123")

        verify(processor).onError(captor.capture())

        assertTrue(captor.value is PushError.Registration)
        assertTrue(captor.value.message.contains("registration failed"))
    }

    @Test
    fun `malformed message exception should not be thrown`() {
        val messageIntent = Intent()
        val bundleExtra = Bundle()
        bundleExtra.putString(MESSAGE_KEY_CHANNEL_ID, "1234")

        val captor = argumentCaptor<EncryptedPushMessage>()
        messageIntent.putExtras(bundleExtra)
        service.onMessage(messageIntent)

        verify(processor, never()).onError(any())
        verify(processor).onMessageReceived(captor.capture())

        assertEquals("1234", captor.value.channelId)
        assertEquals("aes128gcm", captor.value.encoding)
        assertEquals("", captor.value.salt)
        assertEquals("", captor.value.cryptoKey)
    }

    @Test
    fun `processor is not called when no intent message extras provided`() {
        val messageIntent = Intent()
        service.onMessage(messageIntent)

        verifyNoInteractions(processor)
    }

    @Test
    fun `service available reflects Amazon Device Messaging availability`() {
        assertTrue(service.isServiceAvailable(testContext))
    }
}

class TestService : AbstractAmazonPushService()

@RunWith(RobolectricTestRunner::class)
@Config(shadows = [ShadowADMMessageHandlerBase::class, ShadowADM2::class])
class AbstractAmazonPushServiceRegistrationTest {

    private val processor: PushProcessor = mock()
    private val service = TestService()

    @Before
    fun setup() {
        reset(processor)
        PushProcessor.install(processor)
    }

    @Test
    fun `if registrationId does NOT exist startRegister never invokes onNewToken`() {
        val testService = TestService()

        testService.start(testContext)
        verify(processor, never()).onNewToken(anyString())
    }
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
    fun __constructor__(context: Context) {}

    fun isSupported() = true

    fun getRegistrationId() = "123"
}

/**
 * Custom Shadow for [ADM] where the registration ID is null. Currently, we have no way to alter the
 * Shadow class inside of the service, so this is our work around.
 */
@Implements(ADM::class)
class ShadowADM2 {
    @Implementation
    @Suppress("UNUSED_PARAMETER")
    fun __constructor__(context: Context) {}

    fun isSupported() = true

    fun getRegistrationId(): String? = null

    fun startRegister() {}
}
