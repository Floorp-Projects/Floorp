/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.manifest.Size
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.concept.fetch.Client
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class WebAppUseCasesTest {

    @Test
    fun `isInstallable returns false if currentSession has no manifest`() {
        val session: Session = mock()
        `when`(session.securityInfo).thenReturn(Session.SecurityInfo(secure = true))
        `when`(session.webAppManifest).thenReturn(null)

        val sessionManager: SessionManager = mock()
        `when`(sessionManager.selectedSession).thenReturn(session)

        val webAppUseCases = WebAppUseCases(testContext, sessionManager, mock<WebAppShortcutManager>())
        assertFalse(webAppUseCases.isInstallable())
    }

    @Test
    fun `isInstallable returns true if currentSession has a manifest`() {
        val manifest = WebAppManifest(
            name = "Demo",
            startUrl = "https://example.com",
            display = WebAppManifest.DisplayMode.STANDALONE,
            icons = listOf(WebAppManifest.Icon(
                    src = "https://example.com/icon.png",
                    sizes = listOf(Size(192, 192))
            ))
        )

        val session: Session = mock()
        `when`(session.webAppManifest).thenReturn(manifest)
        `when`(session.securityInfo).thenReturn(Session.SecurityInfo(secure = true))

        val sessionManager: SessionManager = mock()
        `when`(sessionManager.selectedSession).thenReturn(session)

        val shortcutManager: WebAppShortcutManager = mock()
        `when`(shortcutManager.supportWebApps).thenReturn(true)

        val webAppUseCases = WebAppUseCases(testContext, sessionManager, shortcutManager)
        assertTrue(webAppUseCases.isInstallable())
    }

    @Suppress("Deprecation")
    @Test
    fun `isInstallable returns false if supportWebApps is false`() {
        val manifest = WebAppManifest(
            name = "Demo",
            startUrl = "https://example.com",
            display = WebAppManifest.DisplayMode.STANDALONE,
            icons = listOf(WebAppManifest.Icon(
                src = "https://example.com/icon.png",
                sizes = listOf(Size(192, 192))
            ))
        )

        val session: Session = mock()
        `when`(session.webAppManifest).thenReturn(manifest)
        `when`(session.securityInfo).thenReturn(Session.SecurityInfo(secure = true))

        val sessionManager: SessionManager = mock()
        `when`(sessionManager.selectedSession).thenReturn(session)

        val shortcutManager: WebAppShortcutManager = mock()
        `when`(shortcutManager.supportWebApps).thenReturn(false)

        assertFalse(WebAppUseCases(testContext, sessionManager, shortcutManager).isInstallable())
        assertFalse(WebAppUseCases(testContext, sessionManager, mock(), supportWebApps = false).isInstallable())
    }

    @Test
    fun `getInstallState returns Installed if manifest exists`() = runBlockingTest {
        val url = "https://mozilla.org"
        val sessionManager: SessionManager = mock()
        val httpClient: Client = mock()
        val storage: ManifestStorage = mock()
        val shortcutManager = WebAppShortcutManager(testContext, httpClient, storage)
        val session: Session = mock()
        val currentTime = System.currentTimeMillis()

        `when`(session.url).thenReturn(url)
        `when`(sessionManager.selectedSession).thenReturn(session)
        `when`(storage.hasRecentManifest(url, currentTime)).thenReturn(true)

        assertEquals(WebAppShortcutManager.WebAppInstallState.Installed, WebAppUseCases(testContext, sessionManager, shortcutManager).getInstallState(currentTime))
    }
}
