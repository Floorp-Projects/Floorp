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
import mozilla.appservices.push.PushException.GeneralException
import mozilla.appservices.push.PushException.MissingRegistrationTokenException
import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.concept.push.EncryptedPushMessage
import mozilla.components.concept.push.PushError
import mozilla.components.concept.push.PushService
import mozilla.components.feature.push.AutoPushFeature.Companion.LAST_VERIFIED
import mozilla.components.feature.push.AutoPushFeature.Companion.PERIODIC_INTERVAL_MILLISECONDS
import mozilla.components.feature.push.AutoPushFeature.Companion.PREFERENCE_NAME
import mozilla.components.feature.push.AutoPushFeature.Companion.PREF_TOKEN
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.nullable
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import mozilla.components.support.test.whenever
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class AutoPushFeatureTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private var lastVerified: Long
        get() = preference(testContext).getLong(LAST_VERIFIED, System.currentTimeMillis())
        set(value) = preference(testContext).edit().putLong(LAST_VERIFIED, value).apply()

    private val connection: PushConnection = mock()

    @Before
    fun setup() {
        whenever(connection.isInitialized()).thenReturn(true)
    }

    @After
    fun shutdown() {
        preference(testContext).edit().remove(LAST_VERIFIED).apply()
    }

    @Test
    fun `initialize starts push service`() {
        val service: PushService = mock()
        val config = PushConfig("push-test")
        val feature = AutoPushFeature(testContext, service, config)

        feature.initialize()

        verify(service).start(testContext)

        verifyNoMoreInteractions(service)
    }

    @Test
    fun `updateToken not called if no token in prefs`() = runTestOnMain {
        AutoPushFeature(testContext, mock(), mock(), coroutineContext, connection)

        verify(connection, never()).updateToken(anyString())
    }

    @Test
    fun `updateToken called if token is in prefs`() = runTestOnMain {
        preference(testContext).edit().putString(PREF_TOKEN, "token").apply()

        val feature = AutoPushFeature(
            testContext,
            mock(),
            mock(),
            connection = connection,
            coroutineContext = coroutineContext,
        )
        feature.initialize()

        verify(connection).updateToken("token")
    }

    @Test
    fun `shutdown stops service and unsubscribes all`() = runTestOnMain {
        val service: PushService = mock()
        whenever(connection.isInitialized()).thenReturn(true)

        AutoPushFeature(testContext, service, mock(), coroutineContext, connection).also {
            it.shutdown()
        }

        verify(connection).unsubscribeAll()
    }

    @Test
    fun `onNewToken updates connection and saves pref`() = runTestOnMain {
        val feature = AutoPushFeature(testContext, mock(), mock(), coroutineContext, connection)

        whenever(connection.subscribe(anyString(), nullable())).thenReturn(mock())

        feature.onNewToken("token")

        verify(connection).updateToken("token")

        val pref = preference(testContext).getString(PREF_TOKEN, null)
        assertNotNull(pref)
        assertEquals("token", pref)
    }

    @Test
    fun `onMessageReceived decrypts message and notifies observers`() = runTestOnMain {
        val encryptedMessage: EncryptedPushMessage = mock()
        val owner: LifecycleOwner = mock()
        val lifecycle: Lifecycle = mock()
        val observer: AutoPushFeature.Observer = mock()
        whenever(owner.lifecycle).thenReturn(lifecycle)
        whenever(lifecycle.currentState).thenReturn(Lifecycle.State.STARTED)
        whenever(encryptedMessage.channelId).thenReturn("992a0f0542383f1ea5ef51b7cf4ae6c4")
        whenever(connection.decryptMessage(any(), any(), any(), any(), any()))
            .thenReturn(null) // If we get null, we shouldn't notify observers.
            .thenReturn(DecryptedMessage("testScope", "test".toByteArray()))

        val feature = AutoPushFeature(testContext, mock(), mock(), coroutineContext, connection)

        feature.register(observer)

        feature.onMessageReceived(encryptedMessage)

        verify(observer, never()).onMessageReceived("testScope", "test".toByteArray())

        feature.onMessageReceived(encryptedMessage)

        verify(observer).onMessageReceived("testScope", "test".toByteArray())
    }

    @Test
    fun `subscribe calls native layer and notifies observers`() = runTestOnMain {
        val connection: PushConnection = mock()
        val subscription: AutoPushSubscription = mock()
        var invoked = false
        val feature = AutoPushFeature(testContext, mock(), mock(), coroutineContext, connection)

        feature.subscribe("testScope") {
            invoked = true
        }

        assertFalse(invoked)

        whenever(connection.isInitialized()).thenReturn(true)
        whenever(connection.subscribe(anyString(), nullable())).thenReturn(subscription)
        whenever(subscription.scope).thenReturn("testScope")

        feature.subscribe("testScope") {
            invoked = true
        }

        assertTrue(invoked)
    }

    @Test
    fun `subscribe invokes error callback`() = runTestOnMain {
        val connection: PushConnection = mock()
        val subscription: AutoPushSubscription = mock()
        var invoked = false
        var errorInvoked = false
        val feature = AutoPushFeature(testContext, mock(), mock(), coroutineContext, connection)

        feature.subscribe(
            scope = "testScope",
            onSubscribeError = {
                errorInvoked = true
            },
            onSubscribe = {
                invoked = true
            },
        )

        assertFalse(invoked)
        assertFalse(errorInvoked)

        whenever(connection.isInitialized()).thenReturn(true)
        whenever(connection.subscribe(anyString(), nullable())).thenAnswer { throw MissingRegistrationTokenException("") }
        whenever(subscription.scope).thenReturn("testScope")

        feature.subscribe(
            scope = "testScope",
            onSubscribeError = {
                errorInvoked = true
            },
            onSubscribe = {
                invoked = true
            },
        )

        assertFalse(invoked)
        assertTrue(errorInvoked)
    }

    @Test
    fun `unsubscribe calls native layer and notifies observers`() = runTestOnMain {
        val connection: PushConnection = mock()
        var invoked = false
        var errorInvoked = false

        val feature = AutoPushFeature(testContext, mock(), mock(), coroutineContext, connection)

        feature.unsubscribe(
            scope = "testScope",
            onUnsubscribeError = {
                errorInvoked = true
            },
            onUnsubscribe = {
                invoked = true
            },
        )

        assertFalse(errorInvoked)
        assertFalse(invoked)

        whenever(connection.unsubscribe(anyString())).thenReturn(false)
        whenever(connection.isInitialized()).thenReturn(true)

        feature.unsubscribe(
            scope = "testScope",
            onUnsubscribeError = {
                errorInvoked = true
            },
            onUnsubscribe = {
                invoked = true
            },
        )

        assertTrue(errorInvoked)
        errorInvoked = false

        whenever(connection.unsubscribe(anyString())).thenReturn(true)

        feature.unsubscribe(
            scope = "testScope",
            onUnsubscribeError = {
                errorInvoked = true
            },
            onUnsubscribe = {
                invoked = true
            },
        )

        assertTrue(invoked)
        assertFalse(errorInvoked)
    }

    @Test
    fun `unsubscribe invokes error callback on native exception`() = runTestOnMain {
        val feature = AutoPushFeature(testContext, mock(), mock(), coroutineContext, connection)
        var invoked = false
        var errorInvoked = false

        whenever(connection.unsubscribe(anyString())).thenAnswer { throw MissingRegistrationTokenException("") }

        feature.unsubscribe(
            scope = "testScope",
            onUnsubscribeError = {
                errorInvoked = true
            },
            onUnsubscribe = {
                invoked = true
            },
        )

        assertFalse(invoked)
        assertTrue(errorInvoked)
    }

    @Test
    fun `getSubscription returns null when there is no subscription`() = runTestOnMain {
        val feature = AutoPushFeature(testContext, mock(), mock(), coroutineContext, connection)
        var invoked = false

        whenever(connection.containsSubscription(anyString())).thenReturn(true)

        feature.getSubscription(
            scope = "testScope",
            appServerKey = null,
        ) {
            invoked = it == null
        }

        assertTrue(invoked)
    }

    @Test
    fun `getSubscription invokes subscribe when there is a subscription`() = runTestOnMain {
        val connection = TestPushConnection(true)
        val feature = AutoPushFeature(testContext, mock(), mock(), coroutineContext, connection)
        var invoked = false

        feature.getSubscription(
            scope = "testScope",
            appServerKey = null,
        ) {
            invoked = it != null
        }

        assertTrue(invoked)
    }

    @Test
    fun `forceRegistrationRenewal deletes pref and calls service`() = runTestOnMain {
        val service: PushService = mock()
        val feature = AutoPushFeature(testContext, service, mock(), coroutineContext, mock())

        feature.renewRegistration()

        verify(service).deleteToken()
        verify(service).start(testContext)

        val pref = preference(testContext).getString(PREF_TOKEN, null)
        assertNull(pref)
    }

    @Test
    fun `verifyActiveSubscriptions notifies observers`() = runTestOnMain {
        val connection: PushConnection = spy(TestPushConnection(true))
        val owner: LifecycleOwner = mock()
        val lifecycle: Lifecycle = mock()
        val observers: AutoPushFeature.Observer = mock()
        val feature = AutoPushFeature(testContext, mock(), mock(), coroutineContext, connection)
        whenever(owner.lifecycle).thenReturn(lifecycle)
        whenever(lifecycle.currentState).thenReturn(Lifecycle.State.STARTED)

        feature.register(observers)

        // When there are NO subscription updates, observers should not be notified.
        feature.verifyActiveSubscriptions()

        verify(observers, never()).onSubscriptionChanged(any())

        // When there are no subscription updates, observers should not be notified.
        whenever(connection.verifyConnection()).thenReturn(emptyList())
        feature.verifyActiveSubscriptions()

        verify(observers, never()).onSubscriptionChanged(any())

        // When there are subscription updates, observers should be notified.
        whenever(connection.verifyConnection()).thenReturn(listOf(AutoPushSubscriptionChanged("scope", "1246")))
        feature.verifyActiveSubscriptions()

        verify(observers).onSubscriptionChanged("scope")
    }

    @Test
    fun `initialize executes verifyActiveSubscriptions after interval`() = runTestOnMain {
        val feature = spy(
            AutoPushFeature(
                context = testContext,
                service = mock(),
                config = mock(),
                coroutineContext = coroutineContext,
                connection = mock(),
            ),
        )

        lastVerified = System.currentTimeMillis() - VERIFY_NOW

        // We want the feature to find a token so it initializes, as that must happen before verify.
        val prefs = testContext.getSharedPreferences("mozac_feature_push", Context.MODE_PRIVATE)
        prefs.edit().putString(PREF_TOKEN, "test-token").apply()

        feature.initialize()

        verify(feature).tryVerifySubscriptions()
    }

    @Test
    fun `initialize does not execute verifyActiveSubscription before interval`() = runTestOnMain {
        val feature = spy(
            AutoPushFeature(
                context = testContext,
                service = mock(),
                config = mock(),
                coroutineContext = coroutineContext,
                connection = mock(),
            ),
        )

        lastVerified = System.currentTimeMillis() - SKIP_INTERVAL

        // We want the feature to find a token so it initializes, as that must happen before verify.
        // For this test, we want the token check to succeed so the ratelimiting check
        // must also work correctly.
        val prefs = testContext.getSharedPreferences("mozac_feature_push", Context.MODE_PRIVATE)
        prefs.edit().putString(PREF_TOKEN, "test-token").apply()

        feature.initialize()

        verify(feature, never()).verifyActiveSubscriptions()
    }

    @Test
    fun `new FCM token executes verifyActiveSubscription`() = runTestOnMain {
        val feature = spy(
            AutoPushFeature(
                context = testContext,
                service = mock(),
                config = mock(),
                coroutineContext = coroutineContext,
                connection = mock(),
            ),
        )

        lastVerified = 0
        feature.initialize()
        // no token yet so should not have even tried.
        verify(feature, never()).verifyActiveSubscriptions()

        // new token == "check now"
        feature.onNewToken("test-token")
        verify(feature).verifyActiveSubscriptions()
    }

    @Test
    fun `verification doesn't happen until we've got the token`() = runTestOnMain {
        val feature = spy(
            AutoPushFeature(
                context = testContext,
                service = mock(),
                config = mock(),
                coroutineContext = coroutineContext,
                connection = mock(),
            ),
        )

        feature.initialize()

        verify(feature, never()).verifyActiveSubscriptions()
    }

    @Test
    fun `crash reporter is notified of errors`() = runTestOnMain {
        val native: PushConnection = TestPushConnection(true)
        val crashReporter: CrashReporting = mock()
        val feature = AutoPushFeature(
            context = testContext,
            service = mock(),
            config = mock(),
            coroutineContext = coroutineContext,
            connection = native,
            crashReporter = crashReporter,
        )

        feature.onError(PushError.Rust(PushError.MalformedMessage("Bad things happened!")))

        verify(crashReporter).submitCaughtException(any<PushError.Rust>())
    }

    @Test
    fun `non-fatal errors are ignored`() = runTestOnMain {
        val crashReporter: CrashReporting = mock()
        val feature = AutoPushFeature(
            context = testContext,
            service = mock(),
            config = mock(),
            coroutineContext = coroutineContext,
            connection = connection,
            crashReporter = crashReporter,
        )

        whenever(connection.unsubscribe(any())).thenAnswer { throw GeneralException("test") }

        feature.unsubscribe("123") {}

        verify(crashReporter, never()).submitCaughtException(any<PushError.Rust>())
    }

    @Test
    fun `only fatal errors are reported`() = runTestOnMain {
        val crashReporter: CrashReporting = mock()
        val feature = AutoPushFeature(
            context = testContext,
            service = mock(),
            config = mock(),
            coroutineContext = coroutineContext,
            connection = connection,
            crashReporter = crashReporter,
        )

        whenever(connection.unsubscribe(any())).thenAnswer { throw MissingRegistrationTokenException("") }

        feature.unsubscribe("123") {}

        verify(crashReporter).submitCaughtException(any<PushError.Rust>())
    }

    companion object {
        private fun preference(context: Context): SharedPreferences {
            return context.getSharedPreferences(PREFERENCE_NAME, Context.MODE_PRIVATE)
        }

        private const val SKIP_INTERVAL = 23 * 60 * 60 * 1000L // 23 hours; less than interval
        private const val VERIFY_NOW = PERIODIC_INTERVAL_MILLISECONDS + (10 * 60 * 1000) // interval + 10 mins
    }

    class TestPushConnection(private val init: Boolean = false) : PushConnection {
        override suspend fun subscribe(scope: String, appServerKey: String?) = AutoPushSubscription(
            scope = scope,
            endpoint = "https://foo",
            publicKey = "p256dh",
            authKey = "auth",
            appServerKey = null,
        )

        override suspend fun unsubscribe(scope: String): Boolean = true

        override suspend fun unsubscribeAll(): Unit = Unit

        override suspend fun containsSubscription(scope: PushScope) = true

        override suspend fun updateToken(token: String) = true

        override suspend fun verifyConnection(): List<AutoPushSubscriptionChanged> = emptyList()

        override suspend fun decryptMessage(
            channelId: String,
            body: String?,
            encoding: String,
            salt: String,
            cryptoKey: String,
        ): DecryptedMessage? = null

        override fun isInitialized() = init

        override fun close() {}
    }
}
