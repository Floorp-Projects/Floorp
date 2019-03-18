/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa

import android.content.pm.ActivityInfo
import android.view.View
import android.view.Window
import android.view.WindowManager
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.manifest.WebAppManifest
import mozilla.components.concept.engine.Engine
import mozilla.components.support.test.mock
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class AbstractWebAppShellActivityTest {
    @Test
    fun `applyConfiguration applies orientation`() {
        val activity = spy(TestWebAppShellActivity())

        val manifest = WebAppManifest(
            name = "Test Manifest",
            startUrl = "/",
            orientation = WebAppManifest.Orientation.LANDSCAPE)

        activity.applyConfiguration(manifest)

        verify(activity).requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_USER_LANDSCAPE
    }

    @Test
    fun `applyConfiguration switches to immersive mode (fullscreen display mode)`() {
        val decorView: View = mock()

        val window: Window = mock()
        doReturn(decorView).`when`(window).decorView

        val activity = spy(TestWebAppShellActivity())
        doReturn(window).`when`(activity).window

        val manifest = WebAppManifest(
            name = "Test Manifest",
            startUrl = "/",
            orientation = WebAppManifest.Orientation.LANDSCAPE,
            display = WebAppManifest.DisplayMode.FULLSCREEN)

        activity.applyConfiguration(manifest)

        verify(window).addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        verify(decorView).systemUiVisibility = (View.SYSTEM_UI_FLAG_LAYOUT_STABLE
            or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
            or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
            or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
            or View.SYSTEM_UI_FLAG_FULLSCREEN
            or View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY)
    }

    @Test
    fun `applyConfiguration does NOT switch to immersive mode (standalone display mode)`() {
        val decorView: View = mock()

        val window: Window = mock()
        doReturn(decorView).`when`(window).decorView

        val activity = spy(TestWebAppShellActivity())
        doReturn(window).`when`(activity).window

        val manifest = WebAppManifest(
            name = "Test Manifest",
            startUrl = "/",
            orientation = WebAppManifest.Orientation.LANDSCAPE,
            display = WebAppManifest.DisplayMode.STANDALONE)

        activity.applyConfiguration(manifest)

        verify(window, never()).addFlags(ArgumentMatchers.anyInt())
        verify(decorView, never()).systemUiVisibility = ArgumentMatchers.anyInt()
    }
}

private class TestWebAppShellActivity(
    override val engine: Engine = mock(),
    override val sessionManager: SessionManager = mock()
) : AbstractWebAppShellActivity()
