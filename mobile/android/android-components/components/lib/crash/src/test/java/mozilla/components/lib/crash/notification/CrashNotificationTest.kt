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
        val foregroundChildNativeCrash = Crash.NativeCodeCrash(
            timestamp = 0,
            minidumpPath = "",
            minidumpSuccess = true,
            extrasPath = "",
            processType = Crash.NativeCodeCrash.PROCESS_TYPE_FOREGROUND_CHILD,
            breadcrumbs = arrayListOf(),
        )

        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(foregroundChildNativeCrash, sdkLevel = 21))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(foregroundChildNativeCrash, sdkLevel = 22))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(foregroundChildNativeCrash, sdkLevel = 23))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(foregroundChildNativeCrash, sdkLevel = 24))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(foregroundChildNativeCrash, sdkLevel = 25))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(foregroundChildNativeCrash, sdkLevel = 26))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(foregroundChildNativeCrash, sdkLevel = 27))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(foregroundChildNativeCrash, sdkLevel = 28))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(foregroundChildNativeCrash, sdkLevel = 29))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(foregroundChildNativeCrash, sdkLevel = 30))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(foregroundChildNativeCrash, sdkLevel = 31))

        val mainProcessNativeCrash = Crash.NativeCodeCrash(
            timestamp = 0,
            minidumpPath = "",
            minidumpSuccess = true,
            extrasPath = "",
            processType = Crash.NativeCodeCrash.PROCESS_TYPE_MAIN,
            breadcrumbs = arrayListOf(),
        )

        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(mainProcessNativeCrash, sdkLevel = 21))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(mainProcessNativeCrash, sdkLevel = 22))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(mainProcessNativeCrash, sdkLevel = 23))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(mainProcessNativeCrash, sdkLevel = 24))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(mainProcessNativeCrash, sdkLevel = 25))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(mainProcessNativeCrash, sdkLevel = 26))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(mainProcessNativeCrash, sdkLevel = 27))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(mainProcessNativeCrash, sdkLevel = 28))
        assertTrue(CrashNotification.shouldShowNotificationInsteadOfPrompt(mainProcessNativeCrash, sdkLevel = 29))
        assertTrue(CrashNotification.shouldShowNotificationInsteadOfPrompt(mainProcessNativeCrash, sdkLevel = 30))
        assertTrue(CrashNotification.shouldShowNotificationInsteadOfPrompt(mainProcessNativeCrash, sdkLevel = 31))

        val backgroundChildNativeCrash = Crash.NativeCodeCrash(
            timestamp = 0,
            minidumpPath = "",
            minidumpSuccess = true,
            extrasPath = "",
            processType = Crash.NativeCodeCrash.PROCESS_TYPE_BACKGROUND_CHILD,
            breadcrumbs = arrayListOf(),
        )

        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(backgroundChildNativeCrash, sdkLevel = 21))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(backgroundChildNativeCrash, sdkLevel = 22))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(backgroundChildNativeCrash, sdkLevel = 23))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(backgroundChildNativeCrash, sdkLevel = 24))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(backgroundChildNativeCrash, sdkLevel = 25))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(backgroundChildNativeCrash, sdkLevel = 26))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(backgroundChildNativeCrash, sdkLevel = 27))
        assertFalse(CrashNotification.shouldShowNotificationInsteadOfPrompt(backgroundChildNativeCrash, sdkLevel = 28))
        assertTrue(CrashNotification.shouldShowNotificationInsteadOfPrompt(backgroundChildNativeCrash, sdkLevel = 29))
        assertTrue(CrashNotification.shouldShowNotificationInsteadOfPrompt(backgroundChildNativeCrash, sdkLevel = 30))
        assertTrue(CrashNotification.shouldShowNotificationInsteadOfPrompt(backgroundChildNativeCrash, sdkLevel = 31))

        val exceptionCrash = Crash.UncaughtExceptionCrash(0, RuntimeException("Boom"), arrayListOf())

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

        val crash = Crash.UncaughtExceptionCrash(0, RuntimeException("Boom"), arrayListOf())

        val crashNotification = CrashNotification(
            testContext,
            crash,
            CrashReporter.PromptConfiguration(
                appName = "TestApp",
            ),
        )
        crashNotification.show()

        assertEquals(1, shadowNotificationManager.notificationChannels.size)
        assertEquals(
            "Crashes",
            (shadowNotificationManager.notificationChannels[0] as NotificationChannel).name,
        )

        assertEquals(1, shadowNotificationManager.size())
    }
}
