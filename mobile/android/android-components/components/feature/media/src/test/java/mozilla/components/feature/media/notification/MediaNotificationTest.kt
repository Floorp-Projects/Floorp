/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.notification

import android.app.Notification
import androidx.core.app.NotificationCompat
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.media.Media
import mozilla.components.feature.media.MockMedia
import mozilla.components.feature.media.R
import mozilla.components.feature.media.state.MediaState
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class MediaNotificationTest {
    @Test
    fun `media notification for playing state`() {
        val state = MediaState.Playing(
            Session("https://www.mozilla.org").apply {
                title = "Mozilla"
            },
            listOf(
                MockMedia(Media.PlaybackState.PLAYING)
            ))

        val notification = MediaNotification(testContext)
            .create(state, mock())

        assertEquals("https://www.mozilla.org", notification.text)
        assertEquals("Mozilla", notification.title)

        assertEquals(R.drawable.mozac_feature_media_playing, notification.iconResource)
    }

    @Test
    fun `media notification for paused state`() {
        val state = MediaState.Paused(
            Session("https://www.mozilla.org").apply {
                title = "Mozilla"
            },
            listOf(
                MockMedia(Media.PlaybackState.PAUSE)
            ))

        val notification = MediaNotification(testContext)
            .create(state, mock())

        assertEquals("https://www.mozilla.org", notification.text)
        assertEquals("Mozilla", notification.title)

        assertEquals(R.drawable.mozac_feature_media_paused, notification.iconResource)
    }

    fun `media notification for none state`() {
        // This notification is only used to call into startForeground() to fulfil the API contract.
        // It gets immediately replaced or removed.

        val state = MediaState.None

        MediaNotification(testContext)
            .create(state, mock())

        val notification = MediaNotification(testContext)
            .create(state, mock())

        assertEquals("", notification.text)
        assertEquals("", notification.title)
    }

    @Test
    fun `media notification for playing state with session without title`() {
        val state = MediaState.Playing(
            Session("https://www.mozilla.org"),
            listOf(
                MockMedia(Media.PlaybackState.PLAYING)
            ))

        val notification = MediaNotification(testContext)
            .create(state, mock())

        assertEquals("https://www.mozilla.org", notification.text)
        assertEquals("https://www.mozilla.org", notification.title)

        assertEquals(R.drawable.mozac_feature_media_playing, notification.iconResource)
    }

    @Test
    fun `media notification for playing state in private mode`() {
        val state = MediaState.Playing(
                Session("https://www.mozilla.org", private = true).apply {
                    title = "Mozilla"
                },
                listOf(
                    MockMedia(Media.PlaybackState.PLAYING)
                ))

        val notification = MediaNotification(testContext)
            .create(state, mock())

        assertEquals("", notification.text)
        assertEquals("A site is playing media", notification.title)

        assertEquals(R.drawable.mozac_feature_media_playing, notification.iconResource)
    }

    @Test
    fun `media notification for paused state in private mode`() {
        val state = MediaState.Paused(
                Session("https://www.mozilla.org", private = true).apply {
                    title = "Mozilla"
                },
                listOf(
                        MockMedia(Media.PlaybackState.PAUSE)
                ))

        val notification = MediaNotification(testContext)
                .create(state, mock())

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