/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.push

import android.util.Base64
import io.mockk.Called
import io.mockk.CapturingSlot
import io.mockk.MockKAnnotations
import io.mockk.Runs
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.just
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.slot
import io.mockk.unmockkStatic
import io.mockk.verify
import kotlinx.coroutines.test.TestScope
import kotlinx.coroutines.test.UnconfinedTestDispatcher
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.webpush.WebPushDelegate
import mozilla.components.concept.engine.webpush.WebPushHandler
import mozilla.components.concept.engine.webpush.WebPushSubscription
import mozilla.components.feature.push.AutoPushFeature
import mozilla.components.feature.push.AutoPushSubscription
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.helpers.MockkRetryTestRule

class WebPushEngineIntegrationTest {

    private val scope = TestScope(UnconfinedTestDispatcher())

    @MockK private lateinit var engine: Engine

    @MockK private lateinit var pushFeature: AutoPushFeature

    @MockK(relaxed = true)
    private lateinit var handler: WebPushHandler
    private lateinit var delegate: CapturingSlot<WebPushDelegate>
    private lateinit var integration: WebPushEngineIntegration

    @get:Rule
    val mockkRule = MockkRetryTestRule()

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        mockkStatic(Base64::class)
        delegate = slot()

        every { engine.registerWebPushDelegate(capture(delegate)) } returns handler
        every { pushFeature.register(any()) } just Runs
        every { pushFeature.unregister(any()) } just Runs
        every { Base64.decode(any<ByteArray>(), any()) } answers { firstArg() }

        integration = WebPushEngineIntegration(engine, pushFeature, scope)
    }

    @After
    fun teardown() {
        unmockkStatic(Base64::class)
    }

    @Test
    fun `methods are no-op before calling start`() = scope.runTest {
        integration.onMessageReceived("push", null)
        integration.onSubscriptionChanged("push")
        verify { handler wasNot Called }

        integration.start()

        integration.onMessageReceived("push", null)
        verify { handler.onPushMessage("push", null) }

        integration.onSubscriptionChanged("push")
        verify { handler.onSubscriptionChanged("push") }
    }

    @Test
    fun `start and stop register and unregister pushFeature`() {
        integration.start()
        verify { pushFeature.register(integration) }

        integration.stop()
        verify { pushFeature.unregister(integration) }
    }

    @Test
    fun `delegate calls getSubscription`() {
        integration.start()
        var subscribeFn: ((AutoPushSubscription?) -> Unit)? = null
        every { pushFeature.getSubscription("scope", block = any()) } answers {
            subscribeFn = thirdArg()
        }

        var actualSubscription: WebPushSubscription? = null
        delegate.captured.onGetSubscription(
            "scope",
            onSubscription = {
                actualSubscription = it
            },
        )

        assertNull(actualSubscription)
        assertNotNull(subscribeFn)

        subscribeFn!!(
            AutoPushSubscription(
                scope = "scope",
                publicKey = "abc",
                endpoint = "def",
                authKey = "xyz",
                appServerKey = null,
            ),
        )

        val expectedSubscription = WebPushSubscription(
            scope = "scope",
            publicKey = "abc".toByteArray(),
            endpoint = "def",
            authSecret = "xyz".toByteArray(),
            appServerKey = null,
        )
        assertEquals(expectedSubscription, actualSubscription)
    }

    @Test
    fun `delegate calls subscribe`() {
        integration.start()
        var onSubscribeErrorFn: ((Exception) -> Unit)? = null
        var onSubscribeFn: ((AutoPushSubscription?) -> Unit)? = null
        every {
            pushFeature.subscribe(
                scope = "scope",
                appServerKey = null,
                onSubscribeError = any(),
                onSubscribe = any(),
            )
        } answers {
            onSubscribeErrorFn = thirdArg()
            onSubscribeFn = lastArg()
        }

        var actualSubscription: WebPushSubscription? = null
        var onSubscribeInvoked = false
        delegate.captured.onSubscribe("scope", null) {
            actualSubscription = it
            onSubscribeInvoked = true
        }
        assertFalse(onSubscribeInvoked)
        assertNull(actualSubscription)

        assertNotNull(onSubscribeErrorFn)
        onSubscribeErrorFn!!(mockk())
        assertTrue(onSubscribeInvoked)
        assertNull(actualSubscription)

        assertNotNull(onSubscribeFn)
        onSubscribeFn!!(
            AutoPushSubscription(
                scope = "scope",
                publicKey = "abc",
                endpoint = "def",
                authKey = "xyz",
                appServerKey = null,
            ),
        )

        val expectedSubscription = WebPushSubscription(
            scope = "scope",
            publicKey = "abc".toByteArray(),
            endpoint = "def",
            authSecret = "xyz".toByteArray(),
            appServerKey = null,
        )

        assertEquals(expectedSubscription, actualSubscription)
    }

    @Test
    fun `delegate calls unsubscribe`() {
        integration.start()
        var onUnsubscribeErrorFn: ((Exception) -> Unit)? = null
        var onUnsubscribeFn: ((Boolean) -> Unit)? = null
        every {
            pushFeature.unsubscribe(
                scope = "scope",
                onUnsubscribeError = any(),
                onUnsubscribe = any(),
            )
        } answers {
            onUnsubscribeErrorFn = secondArg()
            onUnsubscribeFn = thirdArg()
        }

        var onSubscribeInvoked = false
        var unsubscribeSuccess: Boolean? = null
        delegate.captured.onUnsubscribe("scope") {
            onSubscribeInvoked = true
            unsubscribeSuccess = it
        }

        assertFalse(onSubscribeInvoked)
        assertNull(unsubscribeSuccess)

        assertNotNull(onUnsubscribeErrorFn)
        onUnsubscribeErrorFn!!(mockk())
        assertNotNull(unsubscribeSuccess)
        assertFalse(unsubscribeSuccess!!)

        assertNotNull(onUnsubscribeFn)
        onUnsubscribeFn!!(true)
        assertNotNull(unsubscribeSuccess)
        assertTrue(unsubscribeSuccess!!)
    }
}
