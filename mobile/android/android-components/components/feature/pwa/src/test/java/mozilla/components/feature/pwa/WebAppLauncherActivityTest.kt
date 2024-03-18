/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa

import android.content.Intent
import androidx.core.net.toUri
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.feature.pwa.intent.WebAppIntentProcessor.Companion.ACTION_VIEW_PWA
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class WebAppLauncherActivityTest {

    private val baseManifest = WebAppManifest(
        name = "Test",
        startUrl = "https://www.mozilla.org",
    )

    @Test
    fun `DisplayMode-Browser launches browser`() {
        val activity = spy(WebAppLauncherActivity())
        doNothing().`when`(activity).launchBrowser(any())

        val manifest = baseManifest.copy(display = WebAppManifest.DisplayMode.BROWSER)

        activity.routeManifest(manifest.startUrl.toUri(), manifest)

        verify(activity).launchBrowser(manifest.startUrl.toUri())
    }

    @Test
    fun `DisplayMode-minimalui launches web app shell`() {
        val activity = spy(WebAppLauncherActivity())
        doNothing().`when`(activity).launchWebAppShell("https://www.mozilla.org".toUri())

        val manifest = baseManifest.copy(display = WebAppManifest.DisplayMode.MINIMAL_UI)

        activity.routeManifest(manifest.startUrl.toUri(), manifest)

        verify(activity).launchWebAppShell(manifest.startUrl.toUri())
    }

    @Test
    fun `DisplayMode-fullscreen launches web app shell`() {
        val activity = spy(WebAppLauncherActivity())
        doNothing().`when`(activity).launchWebAppShell("https://www.mozilla.org".toUri())

        val manifest = baseManifest.copy(display = WebAppManifest.DisplayMode.FULLSCREEN)

        activity.routeManifest(manifest.startUrl.toUri(), manifest)

        verify(activity).launchWebAppShell(manifest.startUrl.toUri())
    }

    @Test
    fun `DisplayMode-standalone launches web app shell`() {
        val activity = spy(WebAppLauncherActivity())
        doNothing().`when`(activity).launchWebAppShell("https://www.mozilla.org".toUri())

        val manifest = baseManifest.copy(display = WebAppManifest.DisplayMode.STANDALONE)

        activity.routeManifest(manifest.startUrl.toUri(), manifest)

        verify(activity).launchWebAppShell(manifest.startUrl.toUri())
    }

    @Test
    fun `launchBrowser starts activity with VIEW intent`() {
        val activity = spy(WebAppLauncherActivity())
        doReturn("test").`when`(activity).packageName
        doNothing().`when`(activity).startActivity(any())

        val manifest = baseManifest.copy(display = WebAppManifest.DisplayMode.BROWSER)

        activity.launchBrowser(manifest.startUrl.toUri())

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

        val url = "https://example.com".toUri()

        activity.launchWebAppShell(url)

        val captor = argumentCaptor<Intent>()
        verify(activity).startActivity(captor.capture())

        assertEquals(ACTION_VIEW_PWA, captor.value.action)
        assertEquals(url, captor.value.data)
        assertEquals("test", captor.value.`package`)
    }
}
