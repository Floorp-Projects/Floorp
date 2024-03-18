/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.ext

import android.app.Activity
import android.content.pm.ActivityInfo
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.view.reportFullyDrawnSafe
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mockito.ArgumentMatchers.eq
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`

class ActivityKtTest {
    private val baseManifest = WebAppManifest(
        name = "Test Manifest",
        startUrl = "/",
    )

    private lateinit var activity: Activity
    private lateinit var logger: Logger

    @Before
    fun setUp() {
        activity = mock()
        logger = mock()
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
        activity.reportFullyDrawnSafe(logger)
        verify(activity).reportFullyDrawn()
    }

    @Test
    fun `GIVEN reportFullyDrawn throws a SecurityException WHEN reportFullyDrawnSafe is called THEN the exception is caught and a log statement with fully drawn is logged`() {
        val expectedSecurityException = SecurityException()
        `when`(activity.reportFullyDrawn()).thenThrow(expectedSecurityException)
        activity.reportFullyDrawnSafe(logger) // If an exception is thrown, this test will fail.

        val msgArg = argumentCaptor<String>()
        verify(logger).error(msgArg.capture(), eq(expectedSecurityException))
        assertTrue(msgArg.value, msgArg.value.contains("Fully drawn"))
    }
}
