/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.notification

import android.app.Notification
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import androidx.core.app.NotificationCompat
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.MediaSessionState
import mozilla.components.browser.state.state.MediaState
import mozilla.components.browser.state.state.createTab
import mozilla.components.concept.engine.mediasession.MediaSession
import mozilla.components.feature.media.R
import mozilla.components.feature.media.service.AbstractMediaService
import mozilla.components.feature.media.service.AbstractMediaSessionService
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.robolectric.Shadows.shadowOf

@RunWith(AndroidJUnit4::class)
class MediaNotificationTest {
    private lateinit var context: Context

    @Before
    fun setUp() {
        context = spy(testContext).also {
            val packageManager: PackageManager = mock()
            doReturn(Intent()).`when`(packageManager).getLaunchIntentForPackage(ArgumentMatchers.anyString())
            doReturn(packageManager).`when`(it).packageManager
        }
    }

    @Test
    fun `media notification for playing state`() {
        val state = BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "test-tab", title = "Mozilla")
            ),
            media = MediaState(
                aggregate = MediaState.Aggregate(
                    MediaState.State.PLAYING,
                    "test-tab"
                )
            )
        )

        val notification = MediaNotification(context, AbstractMediaService::class.java)
            .create(state, mock())

        assertEquals("https://www.mozilla.org", notification.text)
        assertEquals("Mozilla", notification.title)

        assertEquals(R.drawable.mozac_feature_media_playing, notification.iconResource)

        shadowOf(notification.contentIntent).savedIntent!!.also { intent ->
            assertEquals(AbstractMediaService.ACTION_SWITCH_TAB, intent.action)
            assertTrue(intent.extras!!.containsKey(AbstractMediaService.EXTRA_TAB_ID))
            assertEquals("test-tab", intent.getStringExtra(AbstractMediaService.EXTRA_TAB_ID))
        }
    }

    @Test
    fun `media notification for paused state`() {
        val state = BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "test-tab", title = "Mozilla")
            ),
            media = MediaState(
                aggregate = MediaState.Aggregate(
                    MediaState.State.PAUSED,
                    "test-tab"
                )
            )
        )

        val notification = MediaNotification(context, AbstractMediaService::class.java)
            .create(state, mock())

        assertEquals("https://www.mozilla.org", notification.text)
        assertEquals("Mozilla", notification.title)

        assertEquals(R.drawable.mozac_feature_media_paused, notification.iconResource)

        shadowOf(notification.contentIntent).savedIntent!!.also { intent ->
            assertEquals(AbstractMediaService.ACTION_SWITCH_TAB, intent.action)
            assertTrue(intent.extras!!.containsKey(AbstractMediaService.EXTRA_TAB_ID))
            assertEquals("test-tab", intent.getStringExtra(AbstractMediaService.EXTRA_TAB_ID))
        }
    }

    @Test
    fun `media notification for none state`() {
        // This notification is only used to call into startForeground() to fulfil the API contract.
        // It gets immediately replaced or removed.

        val state = BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "test-tab", title = "Mozilla")
            ),
            media = MediaState(
                aggregate = MediaState.Aggregate(
                    MediaState.State.NONE,
                    "test-tab"
                )
            )
        )

        val notification = MediaNotification(context, AbstractMediaService::class.java)
            .create(state, mock())

        assertEquals("", notification.text)
        assertEquals("", notification.title)
    }

    @Test
    fun `media notification for playing state with session without title`() {
        val state = BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "test-tab")
            ),
            media = MediaState(
                aggregate = MediaState.Aggregate(
                    MediaState.State.PLAYING,
                    "test-tab"
                )
            )
        )

        val notification = MediaNotification(context, AbstractMediaService::class.java)
            .create(state, mock())

        assertEquals("https://www.mozilla.org", notification.text)
        assertEquals("https://www.mozilla.org", notification.title)

        assertEquals(R.drawable.mozac_feature_media_playing, notification.iconResource)
    }

    @Test
    fun `media notification for playing state in private mode`() {
        val state = BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "test-tab", private = true)
            ),
            media = MediaState(
                aggregate = MediaState.Aggregate(
                    MediaState.State.PLAYING,
                    "test-tab"
                )
            )
        )

        val notification = MediaNotification(context, AbstractMediaService::class.java)
            .create(state, mock())

        assertEquals("", notification.text)
        assertEquals("A site is playing media", notification.title)

        assertEquals(R.drawable.mozac_feature_media_playing, notification.iconResource)
    }

    @Test
    fun `media notification for paused state in private mode`() {
        val state = BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "test-tab", private = true)
            ),
            media = MediaState(
                aggregate = MediaState.Aggregate(
                    MediaState.State.PAUSED,
                    "test-tab"
                )
            )
        )

        val notification = MediaNotification(context, AbstractMediaService::class.java)
                .create(state, mock())

        assertEquals("", notification.text)
        assertEquals("A site is playing media", notification.title)

        assertEquals(R.drawable.mozac_feature_media_paused, notification.iconResource)
    }

    @Test
    fun `media session notification for playing state`() {
        val state = BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "test-tab", title = "Mozilla",
                    mediaSessionState = MediaSessionState(mock(), playbackState = MediaSession.PlaybackState.PLAYING)
                ))
        )

        val notification = runBlocking {
            MediaNotification(context, AbstractMediaSessionService::class.java).create(state.tabs[0], mock())
        }

        assertEquals("https://www.mozilla.org", notification.text)
        assertEquals("Mozilla", notification.title)
        assertEquals(R.drawable.mozac_feature_media_playing, notification.iconResource)
    }

    @Test
    fun `media session notification for paused state`() {
        val state = BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "test-tab", title = "Mozilla",
                    mediaSessionState = MediaSessionState(mock(), playbackState = MediaSession.PlaybackState.PAUSED)
                ))
        )

        val notification = runBlocking {
            MediaNotification(context, AbstractMediaSessionService::class.java).create(state.tabs[0], mock())
        }

        assertEquals("https://www.mozilla.org", notification.text)
        assertEquals("Mozilla", notification.title)
        assertEquals(R.drawable.mozac_feature_media_paused, notification.iconResource)
    }

    @Test
    fun `media session notification for stopped state`() {
        val state = BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "test-tab", title = "Mozilla",
                    mediaSessionState = MediaSessionState(mock(), playbackState = MediaSession.PlaybackState.STOPPED)
                ))
        )

        val notification = runBlocking {
            MediaNotification(context, AbstractMediaSessionService::class.java).create(state.tabs[0], mock())
        }

        assertEquals("", notification.text)
        assertEquals("", notification.title)
    }

    @Test
    fun `media session notification for playing state in private mode`() {
        val state = BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "test-tab", title = "Mozilla", private = true,
                    mediaSessionState = MediaSessionState(mock(), playbackState = MediaSession.PlaybackState.PLAYING)
                ))
        )

        val notification = runBlocking {
            MediaNotification(context, AbstractMediaSessionService::class.java).create(state.tabs[0], mock())
        }

        assertEquals("", notification.text)
        assertEquals("A site is playing media", notification.title)
        assertEquals(R.drawable.mozac_feature_media_playing, notification.iconResource)
    }

    @Test
    fun `media session notification for paused state in private mode`() {
        val state = BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "test-tab", title = "Mozilla", private = true,
                    mediaSessionState = MediaSessionState(mock(), playbackState = MediaSession.PlaybackState.PAUSED)
                ))
        )

        val notification = runBlocking {
            MediaNotification(context, AbstractMediaSessionService::class.java).create(state.tabs[0], mock())
        }

        assertEquals("", notification.text)
        assertEquals("A site is playing media", notification.title)
        assertEquals(R.drawable.mozac_feature_media_paused, notification.iconResource)
    }
}

private val Notification.text: String?
    get() = extras.getString(NotificationCompat.EXTRA_TEXT)

private val Notification.title: String?
    get() = extras.getString(NotificationCompat.EXTRA_TITLE)

private val Notification.iconResource: Int
    @Suppress("DEPRECATION")
    get() = icon
