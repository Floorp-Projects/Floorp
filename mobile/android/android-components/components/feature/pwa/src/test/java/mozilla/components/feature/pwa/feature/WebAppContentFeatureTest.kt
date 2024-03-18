/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.feature

import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.CustomTabConfig
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.EngineState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.support.test.mock
import org.junit.Test
import org.mockito.Mockito.verify

class WebAppContentFeatureTest {
    private val customTabId = "custom-id"

    @Test
    fun `display mode is fullscreen based on PWA manifest`() {
        val engineSession = mock<EngineSession>()
        val engineState = EngineState(engineSession = engineSession)

        val tab = CustomTabSessionState(
            id = customTabId,
            content = ContentState("https://mozilla.org"),
            config = CustomTabConfig(),
            engineState = engineState,
        )

        val store = BrowserStore(BrowserState(customTabs = listOf(tab)))
        val manifest = mockManifest(WebAppManifest.DisplayMode.FULLSCREEN)

        val feature = WebAppContentFeature(
            store,
            tabId = tab.id,
            manifest = manifest,
        )
        feature.onCreate(mock())

        verify(engineSession).setDisplayMode(WebAppManifest.DisplayMode.FULLSCREEN)
    }

    private fun mockManifest(display: WebAppManifest.DisplayMode) = WebAppManifest(
        name = "Mock",
        startUrl = "https://mozilla.org",
        scope = "https://mozilla.org",
        display = display,
    )
}
