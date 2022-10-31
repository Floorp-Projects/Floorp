/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.feature

import android.app.Activity
import android.content.pm.ActivityInfo
import android.os.Looper.getMainLooper
import android.view.View
import android.view.Window
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.CompletableDeferred
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.icons.Icon
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`
import org.mockito.MockitoAnnotations.openMocks
import org.robolectric.Shadows.shadowOf

@RunWith(AndroidJUnit4::class)
class WebAppActivityFeatureTest {

    @Mock private lateinit var activity: Activity

    @Mock private lateinit var window: Window

    @Mock private lateinit var decorView: View

    @Mock private lateinit var icons: BrowserIcons

    @Before
    fun setup() {
        openMocks(this)

        `when`(activity.window).thenReturn(window)
        `when`(window.decorView).thenReturn(decorView)
        `when`(icons.loadIcon(any())).thenReturn(CompletableDeferred(mock<Icon>()))
    }

    @Test
    fun `enters immersive mode only when display mode is fullscreen`() {
        val basicManifest = WebAppManifest(
            name = "Demo",
            startUrl = "https://mozilla.com",
            display = WebAppManifest.DisplayMode.STANDALONE,
        )
        WebAppActivityFeature(activity, icons, basicManifest).onResume(mock())

        val fullscreenManifest = basicManifest.copy(
            display = WebAppManifest.DisplayMode.FULLSCREEN,
        )
        WebAppActivityFeature(activity, icons, fullscreenManifest).onResume(mock())
    }

    @Test
    fun `applies orientation`() {
        val manifest = WebAppManifest(
            name = "Test Manifest",
            startUrl = "/",
            orientation = WebAppManifest.Orientation.LANDSCAPE,
        )

        WebAppActivityFeature(activity, icons, manifest).onResume(mock())

        verify(activity).requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_USER_LANDSCAPE
    }

    @Suppress("Deprecation")
    @Test
    fun `sets task description`() {
        val manifest = WebAppManifest(
            name = "Test Manifest",
            startUrl = "/",
        )
        val icon = Icon(mock(), source = Icon.Source.GENERATOR)
        `when`(icons.loadIcon(any())).thenReturn(CompletableDeferred(icon))

        WebAppActivityFeature(activity, icons, manifest).onResume(mock())
        shadowOf(getMainLooper()).idle()

        verify(activity).setTaskDescription(any())
    }
}
