/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.ext

import android.os.Build
import androidx.core.app.NotificationChannelCompat
import androidx.core.app.NotificationManagerCompat
import junit.framework.TestCase.assertFalse
import junit.framework.TestCase.assertTrue
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.util.ReflectionHelpers

@RunWith(RobolectricTestRunner::class)
class NotificationManagerCompatTest {

    private val tested: NotificationManagerCompat = mock()

    @Test
    fun `WHEN areNotificationsEnabled throws an exception THEN areNotificationsEnabledSafe returns false`() {
        whenever(tested.areNotificationsEnabled()).thenThrow(java.lang.RuntimeException())

        assertFalse(tested.areNotificationsEnabledSafe())
    }

    @Test
    fun `WHEN areNotificationsEnabled returns false THEN areNotificationsEnabledSafe returns false`() {
        whenever(tested.areNotificationsEnabled()).thenReturn(false)

        assertFalse(tested.areNotificationsEnabledSafe())
    }

    @Test
    fun `WHEN areNotificationsEnabled returns true THEN areNotificationsEnabledSafe returns true`() {
        whenever(tested.areNotificationsEnabled()).thenReturn(true)

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

        whenever(tested.areNotificationsEnabled()).thenReturn(true)
        whenever(tested.getNotificationChannelCompat(testChannel))
            .thenReturn(notificationChannelCompat)

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

        whenever(tested.areNotificationsEnabled()).thenReturn(true)
        whenever(tested.getNotificationChannelCompat(testChannel))
            .thenReturn(notificationChannelCompat)

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

        whenever(tested.getNotificationChannelCompat(testChannel))
            .thenReturn(notificationChannelCompat)
        whenever(tested.areNotificationsEnabled()).thenReturn(false)

        assertFalse(tested.isNotificationChannelEnabled(testChannel))
    }

    @Test
    fun `WHEN getNotificationChannelCompat returns null THEN isNotificationChannelEnabled returns false`() {
        val testChannel = "test-channel"

        whenever(tested.getNotificationChannelCompat(testChannel)).thenReturn(null)
        whenever(tested.areNotificationsEnabled()).thenReturn(true)

        assertFalse(tested.isNotificationChannelEnabled(testChannel))
    }

    @Test
    fun `WHEN sdk less than 26 and areNotificationsEnabled returns true THEN isNotificationChannelEnabled returns true`() {
        val testChannel = "test-channel"

        ReflectionHelpers.setStaticField(Build.VERSION::class.java, "SDK_INT", 25)

        whenever(tested.areNotificationsEnabled()).thenReturn(true)

        assertTrue(tested.isNotificationChannelEnabled(testChannel))
    }
}
