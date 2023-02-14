/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.os.Build
import androidx.core.app.NotificationChannelCompat
import androidx.core.app.NotificationManagerCompat
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.robolectric.util.ReflectionHelpers

@RunWith(FenixRobolectricTestRunner::class)
class NotificationManagerCompatTest {

    private lateinit var tested: NotificationManagerCompat

    @Before
    fun setup() {
        tested = mockk(relaxed = true)

        mockkStatic("org.mozilla.fenix.ext.NotificationManagerCompatKt")
    }

    @Test
    fun `WHEN areNotificationsEnabled throws an exception THEN areNotificationsEnabledSafe returns false`() {
        every { tested.areNotificationsEnabled() } throws RuntimeException()

        assertFalse(tested.areNotificationsEnabledSafe())
    }

    @Test
    fun `WHEN areNotificationsEnabled returns false THEN areNotificationsEnabledSafe returns false`() {
        every { tested.areNotificationsEnabled() } returns false

        assertFalse(tested.areNotificationsEnabledSafe())
    }

    @Test
    fun `WHEN areNotificationsEnabled returns true THEN areNotificationsEnabledSafe returns true`() {
        every { tested.areNotificationsEnabled() } returns true

        assertTrue(tested.areNotificationsEnabledSafe())
    }

    @Test
    fun `WHEN getNotificationChannelCompat returns a channel with IMPORTANCE_DEFAULT and areNotificationsEnabled returns true THEN isNotificationChannelEnabled returns true`() {
        val testChannel = "test-channel"
        val notificationChannelCompat =
            NotificationChannelCompat.Builder(
                testChannel,
                NotificationManagerCompat.IMPORTANCE_DEFAULT,
            ).build()

        every { tested.getNotificationChannelCompat(testChannel) } returns notificationChannelCompat
        every { tested.areNotificationsEnabled() } returns true

        assertTrue(tested.isNotificationChannelEnabled(testChannel))
    }

    @Test
    fun `WHEN getNotificationChannelCompat returns a channel with IMPORTANCE_NONE and areNotificationsEnabled returns true THEN isNotificationChannelEnabled returns false`() {
        val testChannel = "test-channel"
        val notificationChannelCompat =
            NotificationChannelCompat.Builder(
                testChannel,
                NotificationManagerCompat.IMPORTANCE_NONE,
            ).build()

        every { tested.getNotificationChannelCompat(testChannel) } returns notificationChannelCompat
        every { tested.areNotificationsEnabled() } returns true

        assertFalse(tested.isNotificationChannelEnabled(testChannel))
    }

    @Test
    fun `WHEN getNotificationChannelCompat returns a channel and areNotificationsEnabled returns false THEN isNotificationChannelEnabled returns false`() {
        val testChannel = "test-channel"
        val notificationChannelCompat =
            NotificationChannelCompat.Builder(
                testChannel,
                NotificationManagerCompat.IMPORTANCE_DEFAULT,
            ).build()

        every { tested.getNotificationChannelCompat(testChannel) } returns notificationChannelCompat
        every { tested.areNotificationsEnabled() } returns false

        assertFalse(tested.isNotificationChannelEnabled(testChannel))
    }

    @Test
    fun `WHEN getNotificationChannelCompat returns null THEN isNotificationChannelEnabled returns false`() {
        val testChannel = "test-channel"

        every { tested.getNotificationChannelCompat(testChannel) } returns null
        every { tested.areNotificationsEnabled() } returns true

        assertFalse(tested.isNotificationChannelEnabled(testChannel))
    }

    @Test
    fun `WHEN sdk less than 26 and areNotificationsEnabled returns true THEN isNotificationChannelEnabled returns true`() {
        val testChannel = "test-channel"

        ReflectionHelpers.setStaticField(Build.VERSION::class.java, "SDK_INT", 25)

        every { tested.areNotificationsEnabled() } returns true

        assertTrue(tested.isNotificationChannelEnabled(testChannel))
    }
}
