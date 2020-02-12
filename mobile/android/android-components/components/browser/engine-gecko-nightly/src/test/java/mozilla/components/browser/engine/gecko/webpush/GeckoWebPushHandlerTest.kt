/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.webpush

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.engine.webpush.WebPushSubscription
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.isNull
import org.mockito.Mockito.verify
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.WebPushController

@RunWith(AndroidJUnit4::class)
class GeckoWebPushHandlerTest {

    private val runtime: GeckoRuntime = mock()
    private val controller: WebPushController = mock()

    @Before
    fun setup() {
        `when`(runtime.webPushController).thenReturn(controller)
    }

    @Test
    fun `runtime controller is invoked`() {
        val handler = GeckoWebPushHandler(runtime)
        val subscription = WebPushSubscription(
            "test",
            "https://example.com",
            null,
            ByteArray(65),
            ByteArray(16)
        )

        handler.onPushMessage("", null)

        verify(controller).onPushEvent(any(), isNull())

        handler.onSubscriptionChanged(subscription)

        verify(controller).onSubscriptionChanged(any())

        handler.onSubscriptionChanged(any<String>())

        verify(controller).onSubscriptionChanged(any())
    }
}
