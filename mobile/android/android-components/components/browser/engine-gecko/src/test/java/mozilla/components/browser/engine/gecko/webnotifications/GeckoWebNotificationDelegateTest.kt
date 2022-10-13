/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.webnotifications

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.engine.webnotifications.WebNotification
import mozilla.components.concept.engine.webnotifications.WebNotificationDelegate
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import org.mozilla.geckoview.WebNotification as GeckoViewWebNotification

@RunWith(AndroidJUnit4::class)
class GeckoWebNotificationDelegateTest {

    @Test
    fun `onShowNotification is forwarded to delegate`() {
        val webNotificationDelegate: WebNotificationDelegate = mock()
        val geckoViewWebNotification: GeckoViewWebNotification = mockWebNotification(
            title = "title",
            tag = "tag",
            text = "text",
            imageUrl = "imageUrl",
            textDirection = "textDirection",
            lang = "lang",
            requireInteraction = true,
            source = "source",
        )
        val geckoWebNotificationDelegate = GeckoWebNotificationDelegate(webNotificationDelegate)

        val notificationCaptor = argumentCaptor<WebNotification>()
        geckoWebNotificationDelegate.onShowNotification(geckoViewWebNotification)
        verify(webNotificationDelegate).onShowNotification(notificationCaptor.capture())

        val notification = notificationCaptor.value
        assertEquals(notification.title, geckoViewWebNotification.title)
        assertEquals(notification.tag, geckoViewWebNotification.tag)
        assertEquals(notification.body, geckoViewWebNotification.text)
        assertEquals(notification.sourceUrl, geckoViewWebNotification.source)
        assertEquals(notification.iconUrl, geckoViewWebNotification.imageUrl)
        assertEquals(notification.direction, geckoViewWebNotification.textDirection)
        assertEquals(notification.lang, geckoViewWebNotification.lang)
        assertEquals(notification.requireInteraction, geckoViewWebNotification.requireInteraction)
        assertFalse(notification.triggeredByWebExtension)
    }

    @Test
    fun `onCloseNotification is forwarded to delegate`() {
        val webNotificationDelegate: WebNotificationDelegate = mock()
        val geckoViewWebNotification: GeckoViewWebNotification = mockWebNotification(
            title = "title",
            tag = "tag",
            text = "text",
            imageUrl = "imageUrl",
            textDirection = "textDirection",
            lang = "lang",
            requireInteraction = true,
            source = "source",
        )
        val geckoWebNotificationDelegate = GeckoWebNotificationDelegate(webNotificationDelegate)

        val notificationCaptor = argumentCaptor<WebNotification>()
        geckoWebNotificationDelegate.onCloseNotification(geckoViewWebNotification)
        verify(webNotificationDelegate).onCloseNotification(notificationCaptor.capture())

        val notification = notificationCaptor.value
        assertEquals(notification.title, geckoViewWebNotification.title)
        assertEquals(notification.tag, geckoViewWebNotification.tag)
        assertEquals(notification.body, geckoViewWebNotification.text)
        assertEquals(notification.sourceUrl, geckoViewWebNotification.source)
        assertEquals(notification.iconUrl, geckoViewWebNotification.imageUrl)
        assertEquals(notification.direction, geckoViewWebNotification.textDirection)
        assertEquals(notification.lang, geckoViewWebNotification.lang)
        assertEquals(notification.requireInteraction, geckoViewWebNotification.requireInteraction)
    }

    @Test
    fun `notification without a source are from web extensions`() {
        val webNotificationDelegate: WebNotificationDelegate = mock()
        val geckoViewWebNotification: GeckoViewWebNotification = mockWebNotification(
            title = "title",
            tag = "tag",
            text = "text",
            imageUrl = "imageUrl",
            textDirection = "textDirection",
            lang = "lang",
            requireInteraction = true,
            source = null,
        )
        val geckoWebNotificationDelegate = GeckoWebNotificationDelegate(webNotificationDelegate)

        val notificationCaptor = argumentCaptor<WebNotification>()
        geckoWebNotificationDelegate.onShowNotification(geckoViewWebNotification)
        verify(webNotificationDelegate).onShowNotification(notificationCaptor.capture())

        val notification = notificationCaptor.value
        assertEquals(notification.title, geckoViewWebNotification.title)
        assertEquals(notification.tag, geckoViewWebNotification.tag)
        assertEquals(notification.body, geckoViewWebNotification.text)
        assertEquals(notification.sourceUrl, geckoViewWebNotification.source)
        assertEquals(notification.iconUrl, geckoViewWebNotification.imageUrl)
        assertEquals(notification.direction, geckoViewWebNotification.textDirection)
        assertEquals(notification.lang, geckoViewWebNotification.lang)
        assertEquals(notification.requireInteraction, geckoViewWebNotification.requireInteraction)
        assertTrue(notification.triggeredByWebExtension)
    }
}
