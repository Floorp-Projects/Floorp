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
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineScope
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.webpush.WebPushDelegate
import mozilla.components.concept.engine.webpush.WebPushHandler
import mozilla.components.concept.engine.webpush.WebPushSubscription
import mozilla.components.feature.push.AutoPushFeature
import mozilla.components.feature.push.AutoPushSubscription
import org.junit.After
import org.junit.Before
import org.junit.Test

@ExperimentalCoroutinesApi
class WebPushEngineIntegrationTest {

    private val scope = TestCoroutineScope()
    @MockK private lateinit var engine: Engine
    @MockK private lateinit var pushFeature: AutoPushFeature
    @MockK(relaxed = true) private lateinit var handler: WebPushHandler
    private lateinit var delegate: CapturingSlot<WebPushDelegate>
    private lateinit var integration: WebPushEngineIntegration

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
    fun `methods are no-op before calling start`() = scope.runBlockingTest {
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
        val slot = slot<(AutoPushSubscription?) -> Unit>()
        every { pushFeature.getSubscription("scope", block = capture(slot)) } just Runs

        val onSubscription = mockk<(WebPushSubscription?) -> Unit>(relaxed = true)
        delegate.captured.onGetSubscription("scope", onSubscription)

        verify { onSubscription wasNot Called }
        slot.captured(AutoPushSubscription(
            scope = "scope",
            publicKey = "abc",
            endpoint = "def",
            authKey = "xyz",
            appServerKey = null
        ))

        verify {
            onSubscription(
                WebPushSubscription(
                    scope = "scope",
                    publicKey = "abc".toByteArray(),
                    endpoint = "def",
                    authSecret = "xyz".toByteArray(),
                    appServerKey = null
                )
            )
        }
    }

    @Test
    fun `delegate calls subscribe`() {
        integration.start()
        val onSubscribeError = slot<() -> Unit>()
        val onSubscribe = slot<(AutoPushSubscription?) -> Unit>()
        every {
            pushFeature.subscribe(
                scope = "scope",
                appServerKey = null,
                onSubscribeError = capture(onSubscribeError),
                onSubscribe = capture(onSubscribe)
            )
        } just Runs

        val onSubscription = mockk<(WebPushSubscription?) -> Unit>(relaxed = true)
        delegate.captured.onSubscribe("scope", null, onSubscription)

        verify { onSubscription wasNot Called }

        onSubscribeError.captured()
        verify { onSubscription(null) }

        onSubscribe.captured(AutoPushSubscription(
            scope = "scope",
            publicKey = "abc",
            endpoint = "def",
            authKey = "xyz",
            appServerKey = null
        ))
        verify {
            onSubscription(
                WebPushSubscription(
                    scope = "scope",
                    publicKey = "abc".toByteArray(),
                    endpoint = "def",
                    authSecret = "xyz".toByteArray(),
                    appServerKey = null
                )
            )
        }
    }

    @Test
    fun `delegate calls unsubscribe`() {
        integration.start()
        val onUnsubscribeError = slot<() -> Unit>()
        val onUnsubscribe = slot<(Boolean) -> Unit>()
        every {
            pushFeature.unsubscribe(
                scope = "scope",
                onUnsubscribeError = capture(onUnsubscribeError),
                onUnsubscribe = capture(onUnsubscribe)
            )
        } just Runs

        val onUnsubscription = mockk<(Boolean) -> Unit>(relaxed = true)
        delegate.captured.onUnsubscribe("scope", onUnsubscription)

        verify { onUnsubscription wasNot Called }

        onUnsubscribeError.captured()
        verify { onUnsubscription(false) }

        onUnsubscribe.captured(true)
        verify { onUnsubscription(true) }
    }
}
