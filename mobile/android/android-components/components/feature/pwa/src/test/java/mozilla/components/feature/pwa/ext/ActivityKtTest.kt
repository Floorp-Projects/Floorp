/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.ext

import android.app.Activity
import android.content.pm.ActivityInfo
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.support.ktx.android.view.reportFullyDrawnSafe
import mozilla.components.support.test.mock
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.`when`
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.verify

class ActivityKtTest {
    private val baseManifest = WebAppManifest(
        name = "Test Manifest",
        startUrl = "/"
    )

    private lateinit var activity: Activity

    @Before
    fun setUp() {
        activity = mock()
    }

    @Test
    fun `applyOrientation calls setRequestedOrientation for every value`() {
        WebAppManifest.Orientation.values().forEach { orientation ->
            val activity: Activity = mock()
            activity.applyOrientation(baseManifest.copy(orientation = orientation))
            verify(activity).requestedOrientation = anyInt()
        }
    }

    @Test
    fun `applyOrientation applies common orientations`() {
        run {
            val activity: Activity = mock()
            activity.applyOrientation(baseManifest.copy(orientation = WebAppManifest.Orientation.ANY))
            verify(activity).requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_USER
        }

        run {
            val activity: Activity = mock()
            activity.applyOrientation(baseManifest.copy(orientation = WebAppManifest.Orientation.PORTRAIT))
            verify(activity).requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_USER_PORTRAIT
        }

        run {
            val activity: Activity = mock()
            activity.applyOrientation(baseManifest.copy(orientation = WebAppManifest.Orientation.LANDSCAPE))
            verify(activity).requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_USER_LANDSCAPE
        }

        run {
            val activity: Activity = mock()
            activity.applyOrientation(null)
            verify(activity).requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_USER
        }
    }

    @Test
    fun `WHEN reportFullyDrawnSafe is called THEN reportFullyDrawn is called`() {
        activity.reportFullyDrawnSafe()
        verify(activity).reportFullyDrawn()
    }

    @Test
    fun `GIVEN reportFullyDrawn throws a SecurityException wHEN reportFullyDrawnSafe is called THEN the exception is caught`() {
        `when`(activity.reportFullyDrawn()).thenThrow(SecurityException())
        activity.reportFullyDrawnSafe() // If an exception is thrown, this test will fail.
    }
}
