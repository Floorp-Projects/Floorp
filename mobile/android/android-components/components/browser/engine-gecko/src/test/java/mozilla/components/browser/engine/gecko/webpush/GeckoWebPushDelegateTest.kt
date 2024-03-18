/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.webpush

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.engine.webpush.WebPushDelegate
import mozilla.components.concept.engine.webpush.WebPushSubscription
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.isNull
import org.mockito.Mockito.verify
import org.mozilla.geckoview.GeckoResult

@RunWith(AndroidJUnit4::class)
class GeckoWebPushDelegateTest {

    @Test
    fun `delegate is always invoked`() {
        val delegate: WebPushDelegate = mock()
        val geckoDelegate = GeckoWebPushDelegate(delegate)

        geckoDelegate.onGetSubscription("test")

        verify(delegate).onGetSubscription(eq("test"), any())

        geckoDelegate.onSubscribe("test", null)

        verify(delegate).onSubscribe(eq("test"), isNull(), any())

        geckoDelegate.onSubscribe("test", "key".toByteArray())

        verify(delegate).onSubscribe(eq("test"), eq("key".toByteArray()), any())

        geckoDelegate.onUnsubscribe("test")

        verify(delegate).onUnsubscribe(eq("test"), any())
    }

    @Test
    fun `onGetSubscription result is completed`() {
        var subscription: WebPushSubscription? = WebPushSubscription(
            "test",
            "https://example.com",
            null,
            ByteArray(65),
            ByteArray(16),
        )
        val delegate: WebPushDelegate = object : WebPushDelegate {
            override fun onGetSubscription(
                scope: String,
                onSubscription: (WebPushSubscription?) -> Unit,
            ) {
                onSubscription(subscription)
            }
        }

        val geckoDelegate = GeckoWebPushDelegate(delegate)
        val result = geckoDelegate.onGetSubscription("test")

        result?.accept { sub ->
            assert(sub!!.scope == subscription!!.scope)
        }

        subscription = null

        val nullResult = geckoDelegate.onGetSubscription("test")

        nullResult?.accept { sub ->
            assertNull(sub)
        }
    }

    @Test
    fun `onSubscribe result is completed`() {
        var subscription: WebPushSubscription? = WebPushSubscription(
            "test",
            "https://example.com",
            null,
            ByteArray(65),
            ByteArray(16),
        )
        val delegate: WebPushDelegate = object : WebPushDelegate {
            override fun onSubscribe(
                scope: String,
                serverKey: ByteArray?,
                onSubscribe: (WebPushSubscription?) -> Unit,
            ) {
                onSubscribe(subscription)
            }
        }

        val geckoDelegate = GeckoWebPushDelegate(delegate)
        val result = geckoDelegate.onSubscribe("test", null)

        result?.accept { sub ->
            assert(sub!!.scope == subscription!!.scope)
            assertNull(sub.appServerKey)
        }

        subscription = null

        val nullResult = geckoDelegate.onSubscribe("test", null)
        nullResult?.accept { sub ->
            assertNull(sub)
        }
    }

    @Test
    fun `onUnsubscribe result is completed successfully`() {
        val delegate: WebPushDelegate = object : WebPushDelegate {
            override fun onUnsubscribe(
                scope: String,
                onUnsubscribe: (Boolean) -> Unit,
            ) {
                onUnsubscribe(true)
            }
        }

        val geckoDelegate = GeckoWebPushDelegate(delegate)
        val result = geckoDelegate.onUnsubscribe("test")

        result?.accept { sub ->
            assertNull(sub)
        }
    }

    @Test
    fun `onUnsubscribe result receives throwable when unsuccessful`() {
        val delegate: WebPushDelegate = object : WebPushDelegate {
            override fun onUnsubscribe(
                scope: String,
                onUnsubscribe: (Boolean) -> Unit,
            ) {
                onUnsubscribe(false)
            }
        }

        val geckoDelegate = GeckoWebPushDelegate(delegate)

        val result = geckoDelegate.onUnsubscribe("test")

        result?.exceptionally<Void> { throwable ->
            assertTrue(throwable.localizedMessage == "Un-subscribing from subscription failed.")
            GeckoResult.fromValue(null)
        }
    }
}
