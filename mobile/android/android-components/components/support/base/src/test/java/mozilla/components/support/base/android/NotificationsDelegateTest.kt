/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.android

import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.NotificationManagerCompat
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class NotificationsDelegateTest {

    @Test
    fun `given 2 bound activities, when one of them is unbound, its reference is removed from the notificationPermissionHandler`() {
        val activity = AppCompatActivity()
        val activity2 = AppCompatActivity()

        val notificationManagerCompat: NotificationManagerCompat = mock()

        val notificationsDelegate = NotificationsDelegate(notificationManagerCompat)

        notificationsDelegate.bindToActivity(activity)
        notificationsDelegate.bindToActivity(activity2)

        notificationsDelegate.unBindActivity(activity)

        assertEquals(1, notificationsDelegate.notificationPermissionHandler.size)
        assertEquals(activity2, notificationsDelegate.notificationPermissionHandler.keys.first())
    }

    @Test
    fun `given 2 bound activities, when one of them is garbage collected, its reference is removed from the notificationPermissionHandler`() {
        var activity: AppCompatActivity? = AppCompatActivity()
        val activity2 = AppCompatActivity()

        val notificationManagerCompat: NotificationManagerCompat = mock()

        val notificationsDelegate = NotificationsDelegate(notificationManagerCompat)

        notificationsDelegate.bindToActivity(activity!!)
        notificationsDelegate.bindToActivity(activity2)

        activity = null

        System.gc()

        Thread.sleep(10)

        @Suppress("KotlinConstantConditions")
        assertNull(activity)
        assertEquals(1, notificationsDelegate.notificationPermissionHandler.size)
        assertEquals(activity2, notificationsDelegate.notificationPermissionHandler.keys.first())
    }
}
