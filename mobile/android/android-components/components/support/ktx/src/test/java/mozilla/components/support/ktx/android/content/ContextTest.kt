/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.content

import android.Manifest.permission.WRITE_EXTERNAL_STORAGE
import android.app.ActivityManager
import android.content.Context
import android.content.pm.PackageManager.PERMISSION_GRANTED
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import org.robolectric.Shadows

@RunWith(RobolectricTestRunner::class)
class ContextTest {
    @Test
    fun `systemService() returns same service as getSystemService()`() {
        val context = RuntimeEnvironment.application

        assertEquals(
            context.getSystemService(Context.INPUT_METHOD_SERVICE),
            context.systemService(Context.INPUT_METHOD_SERVICE))

        assertEquals(
            context.getSystemService(Context.ACTIVITY_SERVICE),
            context.systemService(Context.ACTIVITY_SERVICE))

        assertEquals(
            context.getSystemService(Context.LOCATION_SERVICE),
            context.systemService(Context.LOCATION_SERVICE))
    }

    @Test
    fun `isOSOnLowMemory() should return the same as getMemoryInfo() lowMemory`() {
        val context = RuntimeEnvironment.application
        val extensionFunctionResult = context.isOSOnLowMemory()

        val activityManager = context.systemService<ActivityManager>(Context.ACTIVITY_SERVICE)

        val normalMethodResult = ActivityManager.MemoryInfo().also { memoryInfo ->
            activityManager.getMemoryInfo(memoryInfo)
        }.lowMemory

        assertEquals(extensionFunctionResult, normalMethodResult)
    }

    @Test
    fun `isPermissionGranted() returns same service as checkSelfPermission()`() {
        val context = RuntimeEnvironment.application
        val application = Shadows.shadowOf(context)

        assertEquals(
                context.isPermissionGranted(WRITE_EXTERNAL_STORAGE),
                context.checkSelfPermission(WRITE_EXTERNAL_STORAGE) == PERMISSION_GRANTED)

        application.grantPermissions(WRITE_EXTERNAL_STORAGE)

        assertEquals(
                context.isPermissionGranted(WRITE_EXTERNAL_STORAGE),
                context.checkSelfPermission(WRITE_EXTERNAL_STORAGE) == PERMISSION_GRANTED)
    }
}
