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
import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.MediaSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.concept.engine.mediasession.MediaSession
import mozilla.components.feature.media.R
import mozilla.components.feature.media.service.AbstractMediaSessionService
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy

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
    fun `media session notification for playing state`() = runTest {
        val state = BrowserState(
            tabs = listOf(
                createTab(
                    "https://www.mozilla.org",
                    id = "test-tab",
                    title = "Mozilla",
                    mediaSessionState = MediaSessionState(mock(), playbackState = MediaSession.PlaybackState.PLAYING),
                ),
            ),
        )

        val notification = MediaNotification(context, AbstractMediaSessionService::class.java).create(state.tabs[0], mock())

        assertEquals("https://www.mozilla.org", notification.text)
        assertEquals("Mozilla", notification.title)
        assertEquals(R.drawable.mozac_feature_media_playing, notification.iconResource)
    }

    @Test
    fun `media session notification for paused state`() = runTest {
        val state = BrowserState(
            tabs = listOf(
                createTab(
                    "https://www.mozilla.org",
                    id = "test-tab",
                    title = "Mozilla",
                    mediaSessionState = MediaSessionState(mock(), playbackState = MediaSession.PlaybackState.PAUSED),
                ),
            ),
        )

        val notification = MediaNotification(context, AbstractMediaSessionService::class.java).create(state.tabs[0], mock())

        assertEquals("https://www.mozilla.org", notification.text)
        assertEquals("Mozilla", notification.title)
        assertEquals(R.drawable.mozac_feature_media_paused, notification.iconResource)
    }

    @Test
    fun `media session notification for stopped state`() = runTest {
        val state = BrowserState(
            tabs = listOf(
                createTab(
                    "https://www.mozilla.org",
                    id = "test-tab",
                    title = "Mozilla",
                    mediaSessionState = MediaSessionState(mock(), playbackState = MediaSession.PlaybackState.STOPPED),
                ),
            ),
        )

        val notification = MediaNotification(context, AbstractMediaSessionService::class.java).create(state.tabs[0], mock())

        assertEquals("", notification.text)
        assertEquals("", notification.title)
    }

    @Test
    fun `media session notification for playing state in private mode`() = runTest {
        val state = BrowserState(
            tabs = listOf(
                createTab(
                    "https://www.mozilla.org",
                    id = "test-tab",
                    title = "Mozilla",
                    private = true,
                    mediaSessionState = MediaSessionState(mock(), playbackState = MediaSession.PlaybackState.PLAYING),
                ),
            ),
        )

        val notification = MediaNotification(context, AbstractMediaSessionService::class.java).create(state.tabs[0], mock())

        assertEquals("", notification.text)
        assertEquals("A site is playing media", notification.title)
        assertEquals(R.drawable.mozac_feature_media_playing, notification.iconResource)
    }

    @Test
    fun `media session notification for paused state in private mode`() = runTest {
        val state = BrowserState(
            tabs = listOf(
                createTab(
                    "https://www.mozilla.org",
                    id = "test-tab",
                    title = "Mozilla",
                    private = true,
                    mediaSessionState = MediaSessionState(mock(), playbackState = MediaSession.PlaybackState.PAUSED),
                ),
            ),
        )

        val notification = MediaNotification(context, AbstractMediaSessionService::class.java).create(state.tabs[0], mock())

        assertEquals("", notification.text)
        assertEquals("A site is playing media", notification.title)
        assertEquals(R.drawable.mozac_feature_media_paused, notification.iconResource)
    }

    @Test
    fun `media session notification with metadata in non private mode`() = runTest {
        val mediaSessionState: MediaSessionState = mock()
        val metadata: MediaSession.Metadata = mock()
        whenever(mediaSessionState.metadata).thenReturn(metadata)
        whenever(mediaSessionState.playbackState).thenReturn(MediaSession.PlaybackState.PAUSED)
        whenever(metadata.title).thenReturn("test title")

        val state = BrowserState(
            tabs = listOf(
                createTab(
                    "https://www.mozilla.org",
                    id = "test-tab",
                    title = "Mozilla",
                    private = false,
                    mediaSessionState = mediaSessionState,
                ),
            ),
        )

        val notification = MediaNotification(context, AbstractMediaSessionService::class.java).create(state.tabs[0], mock())

        assertEquals("https://www.mozilla.org", notification.text)
        assertEquals("test title", notification.title)
        assertEquals(R.drawable.mozac_feature_media_paused, notification.iconResource)
    }

    @Test
    fun `media session notification with metadata in private mode`() = runTest {
        val mediaSessionState: MediaSessionState = mock()
        val metadata: MediaSession.Metadata = mock()
        whenever(mediaSessionState.metadata).thenReturn(metadata)
        whenever(mediaSessionState.playbackState).thenReturn(MediaSession.PlaybackState.PAUSED)
        whenever(metadata.title).thenReturn("test title")

        val state = BrowserState(
            tabs = listOf(
                createTab(
                    "https://www.mozilla.org",
                    id = "test-tab",
                    title = "Mozilla",
                    private = true,
                    mediaSessionState = mediaSessionState,
                ),
            ),
        )

        val notification = MediaNotification(context, AbstractMediaSessionService::class.java).create(state.tabs[0], mock())

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
