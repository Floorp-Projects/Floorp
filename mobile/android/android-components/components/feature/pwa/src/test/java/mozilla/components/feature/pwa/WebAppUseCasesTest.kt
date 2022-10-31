/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SecurityInfoState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.manifest.Size
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.concept.fetch.Client
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class WebAppUseCasesTest {
    @Test
    fun `isInstallable returns false if currentSession has no manifest`() {
        val session = createTestSession(
            secure = true,
            manifest = null,
        )

        val store = BrowserStore(
            BrowserState(
                tabs = listOf(session),
                selectedTabId = session.id,
            ),
        )

        val webAppUseCases = WebAppUseCases(testContext, store, mock<WebAppShortcutManager>())
        assertFalse(webAppUseCases.isInstallable())
    }

    @Test
    fun `isInstallable returns true if currentSession has a manifest`() {
        val manifest = WebAppManifest(
            name = "Demo",
            startUrl = "https://example.com",
            display = WebAppManifest.DisplayMode.STANDALONE,
            icons = listOf(
                WebAppManifest.Icon(
                    src = "https://example.com/icon.png",
                    sizes = listOf(Size(192, 192)),
                ),
            ),
        )

        val session = createTestSession(secure = true, manifest = manifest)

        val store = BrowserStore(
            BrowserState(
                tabs = listOf(session),
                selectedTabId = session.id,
            ),
        )

        val shortcutManager: WebAppShortcutManager = mock()
        `when`(shortcutManager.supportWebApps).thenReturn(true)

        val webAppUseCases = WebAppUseCases(testContext, store, shortcutManager)
        assertTrue(webAppUseCases.isInstallable())
    }

    @Suppress("Deprecation")
    @Test
    fun `isInstallable returns false if supportWebApps is false`() {
        val manifest = WebAppManifest(
            name = "Demo",
            startUrl = "https://example.com",
            display = WebAppManifest.DisplayMode.STANDALONE,
            icons = listOf(
                WebAppManifest.Icon(
                    src = "https://example.com/icon.png",
                    sizes = listOf(Size(192, 192)),
                ),
            ),
        )

        val session = createTestSession(
            secure = true,
            manifest = manifest,
        )

        val store = BrowserStore(
            BrowserState(
                tabs = listOf(session),
                selectedTabId = session.id,
            ),
        )

        val shortcutManager: WebAppShortcutManager = mock()
        `when`(shortcutManager.supportWebApps).thenReturn(false)

        assertFalse(WebAppUseCases(testContext, store, shortcutManager).isInstallable())
    }

    @Test
    fun `getInstallState returns Installed if manifest exists`() = runTest {
        val httpClient: Client = mock()
        val storage: ManifestStorage = mock()
        val shortcutManager = WebAppShortcutManager(testContext, httpClient, storage)
        val currentTime = System.currentTimeMillis()

        val session = createTestSession(secure = true)
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(session),
                selectedTabId = session.id,
            ),
        )

        `when`(storage.hasRecentManifest("https://www.mozilla.org", currentTime)).thenReturn(true)

        assertEquals(WebAppShortcutManager.WebAppInstallState.Installed, WebAppUseCases(testContext, store, shortcutManager).getInstallState(currentTime))
    }
}

private fun createTestSession(
    secure: Boolean,
    manifest: WebAppManifest? = null,
): TabSessionState {
    val protocol = if (secure) {
        "https"
    } else {
        "http"
    }
    val tab = createTab("$protocol://www.mozilla.org")

    return tab.copy(
        content = tab.content.copy(
            securityInfo = SecurityInfoState(secure = secure),
            webAppManifest = manifest,
        ),
    )
}
