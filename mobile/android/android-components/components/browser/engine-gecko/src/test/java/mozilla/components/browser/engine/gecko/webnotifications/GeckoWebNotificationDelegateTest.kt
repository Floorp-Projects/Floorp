/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.webnotifications

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.engine.webnotifications.WebNotificationDelegate
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doNothing
import java.lang.IllegalStateException
import org.mozilla.geckoview.WebNotification as GeckoViewWebNotification

@RunWith(AndroidJUnit4::class)
class GeckoWebNotificationDelegateTest {

    @Test
    fun `register background message handler`() {
        val webNotificationDelegate: WebNotificationDelegate = mock()
        val geckoViewWebNotification: GeckoViewWebNotification = mock()
        val geckoWebNotificationDelegate = GeckoWebNotificationDelegate(webNotificationDelegate)
        var message: String? = null

        doNothing().`when`(webNotificationDelegate).onShowNotification(any())
        try {
            geckoWebNotificationDelegate.onShowNotification(geckoViewWebNotification)
        } catch (e: IllegalStateException) {
            message = e.localizedMessage
        }

        assertEquals(message, "tag must not be null")
        message = null

        doNothing().`when`(webNotificationDelegate).onCloseNotification(any())
        try {
            geckoWebNotificationDelegate.onCloseNotification(geckoViewWebNotification)
        } catch (e: IllegalStateException) {
            message = e.localizedMessage
        }

        assertEquals(message, "tag must not be null")
    }
}