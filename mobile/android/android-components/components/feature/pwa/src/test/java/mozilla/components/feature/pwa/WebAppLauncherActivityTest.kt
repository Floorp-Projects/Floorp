/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa

import android.content.Intent
import mozilla.components.browser.session.manifest.WebAppManifest
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class WebAppLauncherActivityTest {
    @Test
    fun `DisplayMode-Browser launches browser`() {
        val activity = spy(WebAppLauncherActivity())
        doNothing().`when`(activity).launchBrowser(any())

        val manifest = WebAppManifest(
            name = "Test",
            startUrl = "https://www.mozilla.org",
            display = WebAppManifest.DisplayMode.BROWSER
        )

        activity.routeManifest(manifest)

        verify(activity).launchBrowser(manifest)
    }

    @Test
    fun `DisplayMode-minimalui launches browser`() {
        val activity = spy(WebAppLauncherActivity())
        doNothing().`when`(activity).launchBrowser(any())

        val manifest = WebAppManifest(
            name = "Test",
            startUrl = "https://www.mozilla.org",
            display = WebAppManifest.DisplayMode.MINIMAL_UI
        )

        activity.routeManifest(manifest)

        verify(activity).launchBrowser(manifest)
    }

    @Test
    fun `DisplayMode-fullscreen launches web app shell`() {
        val activity = spy(WebAppLauncherActivity())
        doNothing().`when`(activity).launchWebAppShell()

        val manifest = WebAppManifest(
            name = "Test",
            startUrl = "https://www.mozilla.org",
            display = WebAppManifest.DisplayMode.FULLSCREEN
        )

        activity.routeManifest(manifest)

        verify(activity).launchWebAppShell()
    }

    @Test
    fun `DisplayMode-standalone launches web app shell`() {
        val activity = spy(WebAppLauncherActivity())
        doNothing().`when`(activity).launchWebAppShell()

        val manifest = WebAppManifest(
            name = "Test",
            startUrl = "https://www.mozilla.org",
            display = WebAppManifest.DisplayMode.STANDALONE
        )

        activity.routeManifest(manifest)

        verify(activity).launchWebAppShell()
    }

    @Test
    fun `launchBrowser starts activity with VIEW intent`() {
        val activity = spy(WebAppLauncherActivity())
        doReturn("test").`when`(activity).packageName
        doNothing().`when`(activity).startActivity(any())

        val manifest = WebAppManifest(
            name = "Test",
            startUrl = "https://www.mozilla.org",
            display = WebAppManifest.DisplayMode.BROWSER
        )

        activity.launchBrowser(manifest)

        val captor = argumentCaptor<Intent>()
        verify(activity).startActivity(captor.capture())

        assertEquals(Intent.ACTION_VIEW, captor.value.action)
        assertEquals("https://www.mozilla.org", captor.value.data!!.toString())
        assertEquals("test", captor.value.`package`)
    }

    @Test
    fun `launchWebAppShell starts activity with SHELL intent`() {
        val activity = spy(WebAppLauncherActivity())
        doReturn("test").`when`(activity).packageName
        doNothing().`when`(activity).startActivity(any())

        activity.launchWebAppShell()

        val captor = argumentCaptor<Intent>()
        verify(activity).startActivity(captor.capture())

        assertEquals(AbstractWebAppShellActivity.INTENT_ACTION, captor.value.action)
        assertEquals(Intent.FLAG_ACTIVITY_NEW_TASK, captor.value.flags)
        assertEquals("test", captor.value.`package`)
    }
}
