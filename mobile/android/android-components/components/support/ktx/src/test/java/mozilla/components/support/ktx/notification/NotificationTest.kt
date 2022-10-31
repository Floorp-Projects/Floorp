/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("SameParameterValue")

package mozilla.components.support.ktx.notification

import android.app.NotificationChannel
import android.app.NotificationManager
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import androidx.core.content.getSystemService
import androidx.test.ext.junit.runners.AndroidJUnit4
import junit.framework.TestCase.assertEquals
import junit.framework.TestCase.assertFalse
import junit.framework.TestCase.assertTrue
import mozilla.components.support.ktx.android.notification.ChannelData
import mozilla.components.support.ktx.android.notification.ensureNotificationChannelExists
import mozilla.components.support.test.robolectric.testContext
import org.junit.Test
import org.junit.runner.RunWith

internal const val NOTIFICATION_CHANNEL_ID = "NOTIFICATION_CHANNEL_ID"

@RunWith(AndroidJUnit4::class)
class NotificationTest {

    @Test
    fun `ensureChannelExists - creates a notification channel`() {
        var setupChannelWasCalled = false
        var afterCreatedChannelWasCalled = false

        assertFalse(exists(channelId = NOTIFICATION_CHANNEL_ID))

        val channelData = ChannelData(
            NOTIFICATION_CHANNEL_ID,
            android.R.string.ok,
            NotificationManagerCompat.IMPORTANCE_LOW,
        )

        val setupChannel: NotificationChannel.() -> Unit = {
            assertFalse(exists(channelId = NOTIFICATION_CHANNEL_ID))
            setupChannelWasCalled = true
            lockscreenVisibility = NotificationCompat.VISIBILITY_SECRET
        }

        val afterCreatedChannel: NotificationManager.() -> Unit = {
            assertTrue(exists(channelId = NOTIFICATION_CHANNEL_ID))
            afterCreatedChannelWasCalled = true

            val channel = getChannel(NOTIFICATION_CHANNEL_ID)!!

            assertTrue(channel.lockscreenVisibility == NotificationCompat.VISIBILITY_SECRET)
        }

        val channelId =
            ensureNotificationChannelExists(testContext, channelData, setupChannel, afterCreatedChannel)

        assertTrue(setupChannelWasCalled)
        assertTrue(afterCreatedChannelWasCalled)
        assertTrue(exists(channelId = NOTIFICATION_CHANNEL_ID))
        assertEquals(channelId, NOTIFICATION_CHANNEL_ID)
    }

    private fun exists(channelId: String) = getChannel(channelId) != null

    private fun getChannel(channelId: String): NotificationChannel? {
        val notificationManager = testContext.getSystemService<NotificationManager>()!!
        return notificationManager.getNotificationChannel(channelId)
    }
}
