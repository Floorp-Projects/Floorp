/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.middleware

import android.app.NotificationManager
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.media.RecordingDevice
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.Shadows

@RunWith(AndroidJUnit4::class)
class RecordingDevicesMiddlewareTest {
    @Before
    fun setup() {
        // Prepare the PackageManager to answer getLaunchIntentForPackage call.
        val applicationManager = Shadows.shadowOf(testContext.packageManager)

        val activityComponent = ComponentName(testContext.packageName, "Test")
        applicationManager.addActivityIfNotPresent(activityComponent)

        applicationManager.addIntentFilterForActivity(
            activityComponent,
            IntentFilter(Intent.ACTION_MAIN).apply { addCategory(Intent.CATEGORY_INFO) },
        )
    }

    @Test
    fun `updateNotification should show notification once when recording`() {
        val realNotificationManager = testContext.getSystemService(Context.NOTIFICATION_SERVICE)
            as NotificationManager
        val notificationManager = Shadows.shadowOf(realNotificationManager)

        assertEquals(0, notificationManager.size())

        val middleware = RecordingDevicesMiddleware(testContext)

        middleware.updateNotification(RecordingState.Camera)

        assertEquals(1, notificationManager.size())

        // Another update with the same state to ensure that the notification is shown once.
        middleware.updateNotification(RecordingState.CameraAndMicrophone)

        assertEquals(1, notificationManager.size())
    }

    @Test
    fun `updateNotification hides notification when it has shown notification`() {
        val realNotificationManager = testContext.getSystemService(Context.NOTIFICATION_SERVICE)
            as NotificationManager
        val notificationManager = Shadows.shadowOf(realNotificationManager)

        assertEquals(0, notificationManager.size())

        val middleware = RecordingDevicesMiddleware(testContext)

        middleware.updateNotification(RecordingState.Camera)

        assertEquals(1, notificationManager.size())

        middleware.updateNotification(RecordingState.None)

        assertEquals(0, notificationManager.size())
    }

    @Test
    fun `middleware shows notification when tab has a recording device then hides when recording devices become inactive`() {
        val realNotificationManager = testContext.getSystemService(Context.NOTIFICATION_SERVICE)
            as NotificationManager
        val notificationManager = Shadows.shadowOf(realNotificationManager)

        val middleware = RecordingDevicesMiddleware(testContext)
        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "mozilla"),
                ),
            ),
            middleware = listOf(middleware),
        )

        store.waitUntilIdle()

        assertEquals(0, notificationManager.size())

        store.dispatch(
            ContentAction.SetRecordingDevices(
                sessionId = "mozilla",
                devices = listOf(
                    RecordingDevice(RecordingDevice.Type.CAMERA, RecordingDevice.Status.RECORDING),
                ),
            ),
        ).joinBlocking()

        assertEquals(1, notificationManager.size())

        store.dispatch(
            ContentAction.SetRecordingDevices(
                sessionId = "mozilla",
                devices = emptyList(),
            ),
        ).joinBlocking()

        assertEquals(0, notificationManager.size())
    }
}
