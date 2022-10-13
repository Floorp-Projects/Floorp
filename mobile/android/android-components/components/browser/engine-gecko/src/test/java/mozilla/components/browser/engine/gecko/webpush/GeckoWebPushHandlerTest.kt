/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.webpush

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.isNull
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`
import org.mozilla.geckoview.GeckoRuntime
import org.mozilla.geckoview.WebPushController

@RunWith(AndroidJUnit4::class)
class GeckoWebPushHandlerTest {

    lateinit var runtime: GeckoRuntime
    lateinit var controller: WebPushController

    @Before
    fun setup() {
        controller = mock()
        runtime = mock()
        `when`(runtime.webPushController).thenReturn(controller)
    }

    @Test
    fun `runtime controller is invoked`() {
        val handler = GeckoWebPushHandler(runtime)

        handler.onPushMessage("", null)
        verify(controller).onPushEvent(any(), isNull())

        handler.onSubscriptionChanged("test")
        verify(controller).onSubscriptionChanged(eq("test"))
    }
}
