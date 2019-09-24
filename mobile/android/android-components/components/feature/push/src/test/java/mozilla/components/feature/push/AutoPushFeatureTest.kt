/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.push

import android.content.Context
import android.content.SharedPreferences
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runBlockingTest
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
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class AutoPushFeatureTest {

    @Before
    fun setup() {
        preference(testContext)
            .edit()
            .clear()
            .apply()
    }

    @Test
    fun `initialize starts push service`() {
        val service: PushService = mock()
        val config = PushConfig("push-test")
        val feature = AutoPushFeature(testContext, service, config)

        feature.initialize()

        verify(service).start(testContext)
    }

    @Test
    fun `updateToken not called if no token in prefs`() = runBlockingTest {
        val connection: PushConnection = spy(TestPushConnection())

        spy(AutoPushFeature(testContext, mock(), mock(), coroutineContext, connection))

        verify(connection, never()).updateToken(anyString())
    }

    @Test
    fun `updateToken called if token is in prefs`() = runBlockingTest {
        val connection: PushConnection = spy(TestPushConnection())

        preference(testContext).edit().putString(PREF_TOKEN, "token").apply()

        AutoPushFeature(testContext, mock(), mock(), connection = connection,
            coroutineContext = coroutineContext)

        verify(connection).updateToken("token")
    }

    @Test
    fun `shutdown stops service and unsubscribes all`() = runBlockingTest {
        val service: PushService = mock()
        val connection: PushConnection = mock()
        whenever(connection.isInitialized()).thenReturn(true)

        val feature = spy(AutoPushFeature(testContext, service, mock(), coroutineContext, connection)).also {
            it.shutdown()
        }

        verify(service).stop()
        verify(connection, times(PushType.values().size)).unsubscribe(anyString())
        assertTrue(feature.job.isCancelled)
    }

    @Test
    fun `onNewToken updates connection and saves pref`() = runBlockingTest {
        val connection: PushConnection = mock()
        val feature = spy(AutoPushFeature(testContext, mock(), mock(), coroutineContext, connection))

        feature.onNewToken("token")

        verify(connection).updateToken("token")

        val pref = preference(testContext).getString(PREF_TOKEN, null)
        assertNotNull(pref)
        assertEquals("token", pref)
    }

    @Test
    fun `onNewToken updates subscriptions if token does not already exists`() = runBlockingTest {
        val connection: PushConnection = spy(TestPushConnection(true))
        val feature = spy(AutoPushFeature(testContext, mock(), mock(), coroutineContext, connection))

        feature.onNewToken("token")
        verify(feature, times(1)).subscribeAll()

        feature.onNewToken("token")
        verify(feature, times(1)).subscribeAll()
    }

    @Test
    fun `onMessageReceived decrypts message and notifies observers`() = runBlockingTest {
        val connection: PushConnection = mock()
        val encryptedMessage: EncryptedPushMessage = mock()
        val owner: LifecycleOwner = mock()
        val lifecycle: Lifecycle = mock()
        whenever(owner.lifecycle).thenReturn(lifecycle)
        whenever(lifecycle.currentState).thenReturn(Lifecycle.State.STARTED)
        whenever(connection.isInitialized()).thenReturn(true)
        whenever(encryptedMessage.channelId).thenReturn("992a0f0542383f1ea5ef51b7cf4ae6c4")
        whenever(connection.decrypt(any(), any(), any(), any(), any())).thenReturn("test".toByteArray())

        val feature = spy(AutoPushFeature(testContext, mock(), mock(), coroutineContext, connection))

        feature.registerForPushMessages(PushType.Services, object : Bus.Observer<PushType, String> {
            override fun onEvent(type: PushType, message: String) {
                assertEquals("test", message)
            }
        }, owner, true)

        feature.onMessageReceived(encryptedMessage)
    }

    @Test
    fun `subscribeForType notifies observers`() = runBlockingTest {
        val connection: PushConnection = spy(TestPushConnection(true))
        val owner: LifecycleOwner = mock()
        val lifecycle: Lifecycle = mock()
        whenever(owner.lifecycle).thenReturn(lifecycle)
        whenever(lifecycle.currentState).thenReturn(Lifecycle.State.STARTED)

        val feature = spy(AutoPushFeature(testContext, mock(), mock(), coroutineContext, connection))

        feature.registerForSubscriptions(object : PushSubscriptionObserver {
            override fun onSubscriptionAvailable(subscription: AutoPushSubscription) {
                assertEquals(PushType.Services, subscription.type)
            }
        }, owner, true)

        feature.subscribeForType(PushType.Services)
    }

    @Test
    fun `unsubscribeForType calls rust layer`() = runBlockingTest {
        val connection: PushConnection = mock()
        spy(AutoPushFeature(testContext, mock(), mock(), coroutineContext, connection))
            .unsubscribeForType(PushType.Services)

        verify(connection, never()).unsubscribe(anyString())

        whenever(connection.isInitialized()).thenReturn(true)

        spy(AutoPushFeature(testContext, mock(), mock(), coroutineContext, connection))
            .unsubscribeForType(PushType.Services)

        verify(connection).unsubscribe(anyString())
    }

    @Test
    fun `subscribeAll notifies observers`() = runBlockingTest {
        val connection: PushConnection = spy(TestPushConnection(true))
        val owner: LifecycleOwner = mock()
        val lifecycle: Lifecycle = mock()
        whenever(owner.lifecycle).thenReturn(lifecycle)
        whenever(lifecycle.currentState).thenReturn(Lifecycle.State.STARTED)
        whenever(connection.isInitialized()).thenReturn(true)

        val feature = spy(AutoPushFeature(testContext, mock(), mock(), coroutineContext, connection))

        feature.registerForSubscriptions(object : PushSubscriptionObserver {
            override fun onSubscriptionAvailable(subscription: AutoPushSubscription) {
                assertEquals(PushType.Services, subscription.type)
            }
        }, owner, true)

        feature.subscribeAll()
    }

    @Test
    fun `forceRegistrationRenewal deletes pref and calls service`() = runBlockingTest {
        val service: PushService = mock()
        val feature = spy(AutoPushFeature(testContext, service, mock(), coroutineContext, mock()))

        feature.forceRegistrationRenewal()

        verify(service).deleteToken()

        val pref = preference(testContext).getString(PREF_TOKEN, null)
        assertNull(pref)
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
