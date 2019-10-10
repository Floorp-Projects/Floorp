/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media

import android.app.NotificationManager
import android.content.Context
import android.content.Intent
import android.content.pm.ActivityInfo
import android.content.pm.ResolveInfo
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.media.RecordingDevice
import mozilla.components.concept.engine.media.RecordingDevice.Status.INACTIVE
import mozilla.components.concept.engine.media.RecordingDevice.Status.RECORDING
import mozilla.components.concept.engine.media.RecordingDevice.Type.CAMERA
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.Shadows

@RunWith(AndroidJUnit4::class)
class RecordingDevicesNotificationFeatureTest {

    private lateinit var mockSessionManager: SessionManager
    private lateinit var feature: RecordingDevicesNotificationFeature

    @Before
    fun setup() {
        val context = spy(testContext)
        val engine = mock<Engine>()
        mockSessionManager = spy(SessionManager(engine))

        // Prepare the PackageManager to answer getLaunchIntentForPackage call.
        val applicationManager = Shadows.shadowOf(testContext.packageManager)
        applicationManager.addResolveInfoForIntent(mockedLaunchIntent, mockedResolveInfo)

        feature = RecordingDevicesNotificationFeature(context, mockSessionManager)
    }

    @Test
    fun `enable should register observer inside session manager`() {
        feature.enable()

        verify(mockSessionManager).register(any())
    }

    @Test
    fun `enable should register observer inside session manager's sessions`() {
        val session1 = Session("http://google.com")
        val session2 = Session("http://google.com")
        mockSessionManager.add(session1)
        mockSessionManager.add(session2)

        feature.enable()

        Assert.assertTrue(session1.isObserved())
        Assert.assertTrue(session2.isObserved())
    }

    @Test
    fun `updateRecordingState should show notification once when recording`() {
        val realNotificationManager = testContext.getSystemService(Context.NOTIFICATION_SERVICE)
                as NotificationManager
        val notificationManager = Shadows.shadowOf(realNotificationManager)

        Assert.assertEquals(0, notificationManager.size())

        feature.updateRecordingState(RecordingState.Camera)

        Assert.assertEquals(1, notificationManager.size())

        // Another update with the same state to ensure that the notification is shown once.
        feature.updateRecordingState(RecordingState.CameraAndMicrophone)

        Assert.assertEquals(1, notificationManager.size())
    }

    @Test
    fun `updateRecordingState hides notification when it has shown notification`() {
        val realNotificationManager = testContext.getSystemService(Context.NOTIFICATION_SERVICE)
                as NotificationManager
        val notificationManager = Shadows.shadowOf(realNotificationManager)

        Assert.assertEquals(0, notificationManager.size())

        feature.updateRecordingState(RecordingState.Camera)

        Assert.assertEquals(1, notificationManager.size())

        feature.updateRecordingState(RecordingState.None)

        Assert.assertEquals(0, notificationManager.size())
    }

    @Test
    fun `feature shows notification when session has a recording device then hides when recording devices become inactive`() {
        val realNotificationManager = testContext.getSystemService(Context.NOTIFICATION_SERVICE)
                as NotificationManager
        val notificationManager = Shadows.shadowOf(realNotificationManager)
        val session = Session("http://google.com")
        mockSessionManager.add(session)
        feature.enable()

        // Add a camera device with status recording.
        session.recordingDevices = listOf(RecordingDevice(CAMERA, RECORDING))

        Assert.assertEquals(1, notificationManager.size())

        // Mark the camera device as inactive
        session.recordingDevices = listOf(RecordingDevice(CAMERA, INACTIVE))

        Assert.assertEquals(0, notificationManager.size())
    }

    private val mockedLaunchIntent
        get() = Intent(Intent.ACTION_MAIN).apply {
            addCategory(Intent.CATEGORY_INFO)
            setPackage(testContext.packageName)
        }

    private val mockedResolveInfo
        get() = ResolveInfo().apply {
            activityInfo = ActivityInfo().apply {
                packageName = testContext.packageName
                name = "Name"
            }
        }
}
