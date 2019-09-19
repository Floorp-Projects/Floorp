/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.ext

import android.app.Activity
import android.content.pm.ActivityInfo
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.support.test.mock
import org.junit.Test
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.verify

class ActivityKtTest {
    @Test
    fun `applyOrientation calls setRequestedOrientation for every value`() {
        WebAppManifest.Orientation.values().forEach { orientation ->
            val activity: Activity = mock()
            activity.applyOrientation(
                WebAppManifest(
                    name = "Test Manifest",
                    startUrl = "/",
                    orientation = orientation
                )
            )
            verify(activity).requestedOrientation = anyInt()
        }
    }

    @Test
    fun `applyOrientation applies common orientations`() {
        run {
            val activity: Activity = mock()
            activity.applyOrientation(WebAppManifest(
                name = "Test Manifest",
                startUrl = "/",
                orientation = WebAppManifest.Orientation.ANY))
            verify(activity).requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_USER
        }

        run {
            val activity: Activity = mock()
            activity.applyOrientation(WebAppManifest(
                name = "Test Manifest",
                startUrl = "/",
                orientation = WebAppManifest.Orientation.PORTRAIT))
            verify(activity).requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_USER_PORTRAIT
        }

        run {
            val activity: Activity = mock()
            activity.applyOrientation(WebAppManifest(
                name = "Test Manifest",
                startUrl = "/",
                orientation = WebAppManifest.Orientation.LANDSCAPE))
            verify(activity).requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_USER_LANDSCAPE
        }

        run {
            val activity: Activity = mock()
            activity.applyOrientation(null)
            verify(activity).requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_USER
        }
    }
}
