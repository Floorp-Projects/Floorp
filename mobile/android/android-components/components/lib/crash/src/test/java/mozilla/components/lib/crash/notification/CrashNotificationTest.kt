/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.notification

import android.app.NotificationChannel
import android.app.NotificationManager
import android.content.Context
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.lib.crash.Crash
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.Shadows.shadowOf

@RunWith(AndroidJUnit4::class)
class CrashNotificationTest {
    @Test
    fun shouldShowNotificationInsteadOfPrompt() {
        val nonFatalNativeCrash = Crash.NativeCodeCrash(
            minidumpPath = "",
            minidumpSuccess = true,
            extrasPath = "",
            isFatal = false,
            breadcrumbs = arrayListOf()
        )

        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(nonFatalNativeCrash, sdkLevel = 21))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(nonFatalNativeCrash, sdkLevel = 22))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(nonFatalNativeCrash, sdkLevel = 23))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(nonFatalNativeCrash, sdkLevel = 24))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(nonFatalNativeCrash, sdkLevel = 25))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(nonFatalNativeCrash, sdkLevel = 26))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(nonFatalNativeCrash, sdkLevel = 27))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(nonFatalNativeCrash, sdkLevel = 28))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(nonFatalNativeCrash, sdkLevel = 29))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(nonFatalNativeCrash, sdkLevel = 30))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(nonFatalNativeCrash, sdkLevel = 31))

        val fatalNativeCrash = Crash.NativeCodeCrash(
            minidumpPath = "",
            minidumpSuccess = true,
            extrasPath = "",
            isFatal = true,
            breadcrumbs = arrayListOf()
        )

        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(fatalNativeCrash, sdkLevel = 21))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(fatalNativeCrash, sdkLevel = 22))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(fatalNativeCrash, sdkLevel = 23))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(fatalNativeCrash, sdkLevel = 24))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(fatalNativeCrash, sdkLevel = 25))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(fatalNativeCrash, sdkLevel = 26))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(fatalNativeCrash, sdkLevel = 27))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(fatalNativeCrash, sdkLevel = 28))
        assertTrue(CrashNotification.shouldShowNotificationInsteadOfPrompt(fatalNativeCrash, sdkLevel = 29))
        assertTrue(CrashNotification.shouldShowNotificationInsteadOfPrompt(fatalNativeCrash, sdkLevel = 30))
        assertTrue(CrashNotification.shouldShowNotificationInsteadOfPrompt(fatalNativeCrash, sdkLevel = 31))

        val exceptionCrash = Crash.UncaughtExceptionCrash(RuntimeException("Boom"), arrayListOf())

        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(exceptionCrash, sdkLevel = 21))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(exceptionCrash, sdkLevel = 22))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(exceptionCrash, sdkLevel = 23))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(exceptionCrash, sdkLevel = 24))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(exceptionCrash, sdkLevel = 25))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(exceptionCrash, sdkLevel = 26))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(exceptionCrash, sdkLevel = 27))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(exceptionCrash, sdkLevel = 28))
        assertTrue(CrashNotification.shouldShowNotificationInsteadOfPrompt(exceptionCrash, sdkLevel = 29))
        assertTrue(CrashNotification.shouldShowNotificationInsteadOfPrompt(exceptionCrash, sdkLevel = 30))
        assertTrue(CrashNotification.shouldShowNotificationInsteadOfPrompt(exceptionCrash, sdkLevel = 31))
    }

    @Test
    fun `Showing notification`() {
        val notificationManager = testContext.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        val shadowNotificationManager = shadowOf(notificationManager)

        assertEquals(0, shadowNotificationManager.notificationChannels.size)
        assertEquals(0, shadowNotificationManager.size())

        val crash = Crash.UncaughtExceptionCrash(RuntimeException("Boom"), arrayListOf())

        val crashNotification = CrashNotification(testContext, crash, CrashReporter.PromptConfiguration(
            appName = "TestApp"
        ))
        crashNotification.show()

        assertEquals(1, shadowNotificationManager.notificationChannels.size)
        assertEquals("Crashes",
            (shadowNotificationManager.notificationChannels[0] as NotificationChannel).name)

        assertEquals(1, shadowNotificationManager.size())
    }
}
