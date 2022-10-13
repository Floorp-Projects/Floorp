/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.webnotifications

import android.app.Notification
import android.app.Notification.BigTextStyle
import android.app.Notification.EXTRA_SUB_TEXT
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.icons.Icon
import mozilla.components.browser.icons.IconRequest
import mozilla.components.browser.icons.IconRequest.Resource
import mozilla.components.browser.icons.IconRequest.Resource.Type.MANIFEST_ICON
import mozilla.components.browser.icons.IconRequest.Size.DEFAULT
import mozilla.components.concept.engine.webnotifications.WebNotification
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.verify

private const val TEST_TITLE = "test title"
private const val TEST_TAG = "test tag"
private const val TEST_TEXT = "test text"
private const val TEST_URL = "mozilla.org"
private const val TEST_CHANNEL = "testChannel"

@ExperimentalCoroutinesApi // for runTest
@RunWith(AndroidJUnit4::class)
class NativeNotificationBridgeTest {
    private val blankNotification = WebNotification(
        TEST_TITLE, TEST_TAG, TEST_TEXT, TEST_URL, null, null,
        null, true, mock(), 0,
    )

    private lateinit var icons: BrowserIcons
    private lateinit var bridge: NativeNotificationBridge

    @Before
    fun setup() {
        icons = mock()
        bridge = NativeNotificationBridge(icons, android.R.drawable.ic_dialog_alert)

        val mockIcon = Icon(mock(), source = Icon.Source.GENERATOR)
        doReturn(CompletableDeferred(mockIcon)).`when`(icons).loadIcon(any())
    }

    @Test
    fun `create blank notification`() = runTest {
        val notification = bridge.convertToAndroidNotification(
            blankNotification,
            testContext,
            TEST_CHANNEL,
            null,
            0,
        )

        assertNull(notification.actions)
        assertEquals(TEST_CHANNEL, notification.channelId)
        assertEquals(0, notification.`when`)
        assertNotNull(notification.smallIcon)
        assertNull(notification.getLargeIcon())
        assertTrue(notification.extras.containsKey(EXTRA_SUB_TEXT))
    }

    @Test
    fun `set when`() = runTest {
        val notification = bridge.convertToAndroidNotification(
            blankNotification.copy(timestamp = 1234567890),
            testContext,
            TEST_CHANNEL,
            null,
            0,
        )

        assertEquals(1234567890, notification.`when`)
    }

    @Test
    fun `icon is loaded from BrowserIcons`() = runTest {
        bridge.convertToAndroidNotification(
            blankNotification.copy(sourceUrl = "https://example.com", iconUrl = "https://example.com/large.png"),
            testContext,
            TEST_CHANNEL,
            null,
            0,
        )

        verify(icons).loadIcon(
            IconRequest(
                url = "https://example.com",
                size = DEFAULT,
                resources = listOf(
                    Resource(
                        url = "https://example.com/large.png",
                        type = MANIFEST_ICON,
                    ),
                ),
                isPrivate = true,
            ),
        )
    }

    @Test
    fun `android notification sets BigTextStyle`() = runTest {
        val notification = bridge.convertToAndroidNotification(
            blankNotification.copy(iconUrl = "https://example.com/large.png"),
            testContext,
            TEST_CHANNEL,
            null,
            0,
        )

        val expectedStyle = BigTextStyle().javaClass.name
        assertEquals(expectedStyle, notification.extras.getString(Notification.EXTRA_TEMPLATE))

        val noBodyNotification = bridge.convertToAndroidNotification(
            blankNotification.copy(iconUrl = "https://example.com/large.png", body = null),
            testContext,
            TEST_CHANNEL,
            null,
            0,
        )

        assertNotEquals(expectedStyle, noBodyNotification.extras.getString(Notification.EXTRA_TEMPLATE))
    }
}
