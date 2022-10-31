/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.intent

import android.content.Intent
import android.content.Intent.ACTION_VIEW
import androidx.core.net.toUri
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.state.CustomTabConfig
import mozilla.components.browser.state.state.ExternalAppType
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.feature.intent.ext.getSessionId
import mozilla.components.feature.pwa.ManifestStorage
import mozilla.components.feature.pwa.ext.getWebAppManifest
import mozilla.components.feature.pwa.ext.putUrlOverride
import mozilla.components.feature.pwa.intent.WebAppIntentProcessor.Companion.ACTION_VIEW_PWA
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.tabs.CustomTabsUseCases
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`

@RunWith(AndroidJUnit4::class)
@ExperimentalCoroutinesApi
class WebAppIntentProcessorTest {
    @Test
    fun `process checks if intent action is not valid`() {
        val store = BrowserStore()

        val processor = WebAppIntentProcessor(store, mock(), mock(), mock())

        assertFalse(processor.process(Intent(ACTION_VIEW)))
        assertFalse(processor.process(Intent(ACTION_VIEW_PWA, null)))
        assertFalse(processor.process(Intent(ACTION_VIEW_PWA, "".toUri())))
    }

    @Test
    fun `process returns false if no manifest is in storage`() = runTest {
        val storage: ManifestStorage = mock()
        val processor = WebAppIntentProcessor(mock(), mock(), mock(), storage)

        `when`(storage.loadManifest("https://mozilla.com")).thenReturn(null)

        assertFalse(processor.process(Intent(ACTION_VIEW_PWA, "https://mozilla.com".toUri())))
    }

    @Test
    fun `process adds session ID and manifest to intent`() = runTest {
        val store = BrowserStore()
        val storage: ManifestStorage = mock()

        val manifest = WebAppManifest(
            name = "Test Manifest",
            startUrl = "https://mozilla.com",
        )
        `when`(storage.loadManifest("https://mozilla.com")).thenReturn(manifest)

        val addTabUseCase: CustomTabsUseCases.AddWebAppTabUseCase = mock()
        whenever(
            addTabUseCase.invoke(
                url = "https://mozilla.com",
                source = SessionState.Source.Internal.HomeScreen,
                customTabConfig = CustomTabConfig(
                    externalAppType = ExternalAppType.PROGRESSIVE_WEB_APP,
                    enableUrlbarHiding = true,
                    showCloseButton = false,
                    showShareMenuItem = true,

                ),
                webAppManifest = manifest,
            ),
        ).thenReturn("42")

        val processor = WebAppIntentProcessor(store, addTabUseCase, mock(), storage)

        val intent = Intent(ACTION_VIEW_PWA, "https://mozilla.com".toUri())
        assertTrue(processor.process(intent))

        assertNotNull(intent.getSessionId())
        assertEquals("42", intent.getSessionId())
        assertEquals(manifest, intent.getWebAppManifest())
    }

    @Test
    fun `process adds custom tab config`() = runTest {
        val intent = Intent(ACTION_VIEW_PWA, "https://mozilla.com".toUri())

        val storage: ManifestStorage = mock()
        val store = BrowserStore()

        val manifest = WebAppManifest(
            name = "Test Manifest",
            startUrl = "https://mozilla.com",
        )
        `when`(storage.loadManifest("https://mozilla.com")).thenReturn(manifest)

        val addTabUseCase: CustomTabsUseCases.AddWebAppTabUseCase = mock()

        val processor = WebAppIntentProcessor(store, addTabUseCase, mock(), storage)
        assertTrue(processor.process(intent))

        verify(addTabUseCase).invoke(
            url = "https://mozilla.com",
            source = SessionState.Source.Internal.HomeScreen,
            customTabConfig = CustomTabConfig(
                externalAppType = ExternalAppType.PROGRESSIVE_WEB_APP,
                enableUrlbarHiding = true,
                showCloseButton = false,
                showShareMenuItem = true,

            ),
            webAppManifest = manifest,
        )
    }

    @Test
    fun `url override is applied to session if present`() = runTest {
        val store = BrowserStore()

        val storage: ManifestStorage = mock()
        val loadUrlUseCase: SessionUseCases.DefaultLoadUrlUseCase = mock()
        val processor = WebAppIntentProcessor(store, mock(), loadUrlUseCase, storage)
        val urlOverride = "https://mozilla.com/deep/link/index.html"

        val manifest = WebAppManifest(
            name = "Test Manifest",
            startUrl = "https://mozilla.com",
        )

        `when`(storage.loadManifest("https://mozilla.com")).thenReturn(manifest)

        val intent = Intent(ACTION_VIEW_PWA, "https://mozilla.com".toUri())

        intent.putUrlOverride(urlOverride)

        assertTrue(processor.process(intent))
        verify(loadUrlUseCase).invoke(eq(urlOverride), any(), any(), any())
    }
}
