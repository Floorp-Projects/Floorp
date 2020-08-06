/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.content

import android.Manifest.permission.WRITE_EXTERNAL_STORAGE
import android.app.Activity
import android.app.ActivityManager
import android.content.Context
import android.content.Intent
import android.content.Intent.FLAG_ACTIVITY_NEW_TASK
import android.content.pm.PackageManager.PERMISSION_GRANTED
import android.hardware.camera2.CameraManager
import androidx.core.content.getSystemService
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.Robolectric
import org.robolectric.Shadows.shadowOf
import org.robolectric.shadows.ShadowApplication
import org.robolectric.shadows.ShadowCameraCharacteristics
import org.robolectric.shadows.ShadowProcess
import java.lang.IllegalStateException

@RunWith(AndroidJUnit4::class)
class ContextTest {

    @Before
    fun setup() {
        isMainProcess = null
    }

    @Test
    fun `isOSOnLowMemory() should return the same as getMemoryInfo() lowMemory`() {
        val extensionFunctionResult = testContext.isOSOnLowMemory()

        val activityManager: ActivityManager? = testContext.getSystemService()

        val normalMethodResult = ActivityManager.MemoryInfo().also { memoryInfo ->
            activityManager?.getMemoryInfo(memoryInfo)
        }.lowMemory

        assertEquals(extensionFunctionResult, normalMethodResult)
    }

    @Test
    fun `isPermissionGranted() returns same service as checkSelfPermission()`() {
        val application = ShadowApplication()

        assertEquals(
            testContext.isPermissionGranted(WRITE_EXTERNAL_STORAGE),
            testContext.checkSelfPermission(WRITE_EXTERNAL_STORAGE) == PERMISSION_GRANTED)

        application.grantPermissions(WRITE_EXTERNAL_STORAGE)

        assertEquals(
            testContext.isPermissionGranted(WRITE_EXTERNAL_STORAGE),
            testContext.checkSelfPermission(WRITE_EXTERNAL_STORAGE) == PERMISSION_GRANTED)
    }

    @Test
    fun `share invokes startActivity`() {
        val context = spy(testContext)
        val argCaptor = argumentCaptor<Intent>()

        val result = context.share("https://mozilla.org")

        verify(context).startActivity(argCaptor.capture())

        assertTrue(result)
        assertEquals(FLAG_ACTIVITY_NEW_TASK, argCaptor.value.flags)
    }

    @Test
    fun `email invokes startActivity`() {
        val context = spy(testContext)
        val argCaptor = argumentCaptor<Intent>()

        val result = context.email("test@mozilla.org")

        verify(context).startActivity(argCaptor.capture())

        assertTrue(result)
        assertEquals(FLAG_ACTIVITY_NEW_TASK, argCaptor.value.flags)
    }

    @Test
    fun `call invokes startActivity`() {
        val context = spy(testContext)
        val argCaptor = argumentCaptor<Intent>()

        val result = context.call("555-5555")

        verify(context).startActivity(argCaptor.capture())

        assertTrue(result)
        assertEquals(FLAG_ACTIVITY_NEW_TASK, argCaptor.value.flags)
    }

    @Test
    fun `isMainProcess must only return true if we are in the main process`() {
        val myPid = Int.MAX_VALUE

        assertTrue(testContext.isMainProcess())

        ShadowProcess.setPid(myPid)
        isMainProcess = null

        assertFalse(testContext.isMainProcess())
    }

    @Test
    fun `runOnlyInMainProcess must only run if we are in the main process`() {
        val myPid = Int.MAX_VALUE
        var wasExecuted = false

        testContext.runOnlyInMainProcess {
            wasExecuted = true
        }

        assertTrue(wasExecuted)

        wasExecuted = false
        ShadowProcess.setPid(myPid)
        isMainProcess = false

        testContext.runOnlyInMainProcess {
            wasExecuted = true
        }

        assertFalse(wasExecuted)
    }

    @Test
    fun `hasCamera returns true if the device has a camera`() {
        val context = Robolectric.buildActivity(Activity::class.java).get()
        assertFalse(context.hasCamera())

        val cameraManager: CameraManager = context.getSystemService(Context.CAMERA_SERVICE) as CameraManager
        shadowOf(cameraManager).addCamera("camera0", ShadowCameraCharacteristics.newCameraCharacteristics())
        assertTrue(context.hasCamera())
    }

    @Test
    fun `hasCamera returns false if exception is thrown`() {
        val context = spy(testContext)
        val cameraManager: CameraManager = mock()
        whenever(context.getSystemService(Context.CAMERA_SERVICE)).thenReturn(cameraManager)
        whenever(cameraManager.cameraIdList).thenThrow(IllegalStateException("Test"))
        assertFalse(context.hasCamera())
    }
}
