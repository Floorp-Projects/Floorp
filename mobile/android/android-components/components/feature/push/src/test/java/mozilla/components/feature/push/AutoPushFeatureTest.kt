/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.feature.push

import android.content.Context
import android.content.SharedPreferences
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.test.core.app.ApplicationProvider
import kotlinx.coroutines.runBlocking
import mozilla.appservices.push.KeyInfo
import mozilla.appservices.push.SubscriptionInfo
import mozilla.appservices.push.SubscriptionResponse
import mozilla.components.concept.push.Bus
import mozilla.components.concept.push.EncryptedPushMessage
import mozilla.components.concept.push.PushService
import mozilla.components.feature.push.AutoPushFeature.Companion.PREFERENCE_NAME
import mozilla.components.feature.push.AutoPushFeature.Companion.PREF_TOKEN
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.`when`
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class AutoPushFeatureTest {

    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    @Before
    fun setup() {
        preference(context)
            .edit()
            .clear()
            .apply()
    }

    @Test
    fun `initialize starts push service`() {
        val service: PushService = mock()
        val config = PushConfig("push-test")
        val feature = AutoPushFeature(context, service, config)

        feature.initialize()

        verify(service).start(context)
    }

    @Test
    fun `updateToken not called if no token in prefs`() {
        val connection: PushConnection = spy(TestPushConnection())

        spy(AutoPushFeature(context, mock(), mock(), connection = connection))

        runBlocking {
            verify(connection, never()).updateToken(anyString())
        }
    }

    @Test
    fun `updateToken called if token is in prefs`() {
        val connection: PushConnection = spy(TestPushConnection())

        preference(context).edit().putString(PREF_TOKEN, "token").apply()

        spy(AutoPushFeature(context, mock(), mock(), connection = connection))

        runBlocking {
            verify(connection).updateToken("token")
        }
    }

    @Test
    fun `shutdown stops service and unsubscribes all`() {
        val service: PushService = mock()
        val connection: PushConnection = mock()
        `when`(connection.isInitialized()).thenReturn(true)
        val feature = runBlocking {
            spy(AutoPushFeature(context, service, mock(), coroutineContext, connection)).also {
                it.shutdown()
            }
        }
        runBlocking {
            verify(service).stop()
            verify(connection, times(PushType.values().size)).unsubscribe(anyString())
            assertTrue(feature.job.isCancelled)
        }
    }

    @Test
    fun `onNewToken updates connection and saves pref`() {
        val connection: PushConnection = mock()
        runBlocking {
            val feature = spy(AutoPushFeature(context, mock(), mock(), coroutineContext, connection))

            feature.onNewToken("token")
        }
        runBlocking {
            verify(connection).updateToken("token")
        }
        val pref = preference(context).getString(PREF_TOKEN, null)
        assertNotNull(pref)
        assertEquals("token", pref)
    }

    @Ignore
    @Test
    fun `onNewToken updates subscriptions based on pref`() {
        val connection: PushConnection = spy(TestPushConnection(true))
        runBlocking {
            val feature = spy(AutoPushFeature(context, mock(), mock(), coroutineContext, connection))

            feature.onNewToken("token")
            verify(feature).subscribeAll()

            feature.onNewToken("token")
            verify(feature, never()).subscribeAll()
        }
    }

    @Test
    fun `onMessageReceived decrypts message and notifies observers`() {
        val connection: PushConnection = mock()
        val encryptedMessage: EncryptedPushMessage = mock()
        val owner: LifecycleOwner = mock()
        val lifecycle: Lifecycle = mock()
        `when`(owner.lifecycle).thenReturn(lifecycle)
        `when`(lifecycle.currentState).thenReturn(Lifecycle.State.STARTED)
        `when`(connection.isInitialized()).thenReturn(true)
        `when`(encryptedMessage.channelId).thenReturn("992a0f0542383f1ea5ef51b7cf4ae6c4")
        `when`(connection.decrypt(any(), any(), any(), any(), any())).thenReturn("test".toByteArray())
        runBlocking {
            val feature = spy(AutoPushFeature(context, mock(), mock(), coroutineContext, connection))

            feature.registerForPushMessages(PushType.Services, object : Bus.Observer<PushType, String> {
                override fun onEvent(type: PushType, message: String) {
                    assertEquals("test", message)
                }
            }, owner, true)

            feature.onMessageReceived(encryptedMessage)
        }
    }

    @Test
    fun `subscribeForType notifies observers`() {
        val connection: PushConnection = spy(TestPushConnection(true))
        val owner: LifecycleOwner = mock()
        val lifecycle: Lifecycle = mock()
        `when`(owner.lifecycle).thenReturn(lifecycle)
        `when`(lifecycle.currentState).thenReturn(Lifecycle.State.STARTED)
        runBlocking {
            val feature = spy(AutoPushFeature(context, mock(), mock(), coroutineContext, connection))

            feature.registerForSubscriptions(object : PushSubscriptionObserver {
                override fun onSubscriptionAvailable(subscription: AutoPushSubscription) {
                    assertEquals(PushType.Services, subscription.type)
                }
            }, owner, true)

            feature.subscribeForType(PushType.Services)
        }
    }

    @Test
    fun `unsubscribeForType calls rust layer`() {
        val connection: PushConnection = mock()
        runBlocking {
            val feature = spy(AutoPushFeature(context, mock(), mock(), coroutineContext, connection))
            feature.unsubscribeForType(PushType.Services)
        }

        runBlocking {
            verify(connection, never()).unsubscribe(anyString())
        }

        `when`(connection.isInitialized()).thenReturn(true)

        runBlocking {
            val feature = spy(AutoPushFeature(context, mock(), mock(), coroutineContext, connection))
            feature.unsubscribeForType(PushType.Services)
        }

        runBlocking {
            verify(connection).unsubscribe(anyString())
        }
    }

    @Test
    fun `subscribeAll notifies observers`() {
        val connection: PushConnection = spy(TestPushConnection(true))
        val owner: LifecycleOwner = mock()
        val lifecycle: Lifecycle = mock()
        `when`(owner.lifecycle).thenReturn(lifecycle)
        `when`(lifecycle.currentState).thenReturn(Lifecycle.State.STARTED)
        `when`(connection.isInitialized()).thenReturn(true)
        runBlocking {
            val feature = spy(AutoPushFeature(context, mock(), mock(), coroutineContext, connection))

            feature.registerForSubscriptions(object : PushSubscriptionObserver {
                override fun onSubscriptionAvailable(subscription: AutoPushSubscription) {
                    assertEquals(PushType.Services, subscription.type)
                }
            }, owner, true)

            feature.subscribeAll()
        }
    }

    private fun preference(context: Context): SharedPreferences {
        return context.getSharedPreferences(PREFERENCE_NAME, Context.MODE_PRIVATE)
    }

    class TestPushConnection(private val init: Boolean = false) : PushConnection {
        override suspend fun subscribe(channelId: String, scope: String) =
            SubscriptionResponse(
                "992a0f0542383f1ea5ef51b7cf4ae6c4",
                SubscriptionInfo("https://foo", KeyInfo("auth", "p256dh"))
            )

        override suspend fun unsubscribe(channelId: String): Boolean = true

        override suspend fun unsubscribeAll(): Boolean = true

        override suspend fun updateToken(token: String) = true

        override suspend fun verifyConnection(): Boolean {
            TODO("not implemented")
        }

        override fun decrypt(
            channelId: String,
            body: String,
            encoding: String,
            salt: String,
            cryptoKey: String
        ): ByteArray {
            TODO("not implemented")
        }

        override fun isInitialized() = init

        override fun close() {}
    }
}
