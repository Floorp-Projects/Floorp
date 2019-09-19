/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.webnotifications

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.icons.Icon
import mozilla.components.browser.icons.IconRequest
import mozilla.components.concept.engine.webnotifications.WebNotification
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
@ExperimentalCoroutinesApi
class NativeNotificationBridgeTest {

    private val blankNotificaiton = WebNotification(
        origin = "https://example.com",
        onClick = {},
        onClose = {}
    )

    private lateinit var icons: BrowserIcons
    private lateinit var bridge: NativeNotificationBridge

    @Before
    fun setup() {
        icons = mock()
        bridge = NativeNotificationBridge(icons)

        val mockIcon = Icon(mock(), source = Icon.Source.GENERATOR)
        doReturn(CompletableDeferred(mockIcon)).`when`(icons).loadIcon(any())
    }

    @Test
    fun `create blank notification`() = runBlockingTest {
        val notification = bridge.convertToAndroidNotification(
            blankNotificaiton,
            testContext,
            "channel"
        )

        assertNull(notification.actions)
        @Suppress("Deprecation")
        assertArrayEquals(longArrayOf(), notification.vibrate)
        assertEquals(0, notification.`when`)
        assertEquals("channel", notification.channelId)
        assertNull(notification.getLargeIcon())
        assertNull(notification.smallIcon)
    }

    @Test
    fun `set vibration pattern`() = runBlockingTest {
        val notification = bridge.convertToAndroidNotification(
            blankNotificaiton.copy(vibrate = longArrayOf(1, 2, 3)),
            testContext,
            "channel"
        )

        @Suppress("Deprecation")
        assertArrayEquals(longArrayOf(1, 2, 3), notification.vibrate)
    }

    @Test
    fun `set when`() = runBlockingTest {
        val notification = bridge.convertToAndroidNotification(
            blankNotificaiton.copy(timestamp = 1234567890),
            testContext,
            "channel"
        )

        assertEquals(1234567890, notification.`when`)
    }

    @Test
    fun `icon is loaded from BrowserIcons`() = runBlockingTest {
        bridge.convertToAndroidNotification(
            blankNotificaiton.copy(iconUrl = "https://example.com/large.png"),
            testContext,
            "channel"
        )

        verify(icons).loadIcon(
            IconRequest(
                url = "https://example.com",
                size = IconRequest.Size.DEFAULT,
                resources = listOf(IconRequest.Resource(
                    url = "https://example.com/large.png",
                    type = IconRequest.Resource.Type.MANIFEST_ICON
                ))
            )
        )
    }
}
