/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.feature

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.feature.pwa.ManifestStorage
import mozilla.components.feature.pwa.WebAppShortcutManager
import mozilla.components.support.test.any
import mozilla.components.support.test.robolectric.testContext
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.Mockito.`when`
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.MockitoAnnotations.initMocks

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class ManifestUpdateFeatureTest {

    @Mock private lateinit var sessionManager: SessionManager
    @Mock private lateinit var session: Session
    @Mock private lateinit var shortcutManager: WebAppShortcutManager
    @Mock private lateinit var storage: ManifestStorage
    private val sessionId = "external-app-session-id"

    @Before
    fun setup() {
        initMocks(this)
        `when`(sessionManager.findSessionById(sessionId)).thenReturn(session)
    }

    @Test
    fun `start and stop controls session observer`() {
        val manifest = WebAppManifest(name = "Mozilla", startUrl = "https://mozilla.org")
        val feature = ManifestUpdateFeature(testContext, sessionManager, shortcutManager, storage, sessionId, manifest)

        feature.start()
        verify(session).register(feature)

        feature.stop()
        verify(session).register(feature)
    }

    @Test
    fun `start and stop handle null session`() {
        val manifest = WebAppManifest(name = "Mozilla", startUrl = "https://mozilla.org")
        `when`(sessionManager.findSessionById(sessionId)).thenReturn(null)
        val feature = ManifestUpdateFeature(testContext, sessionManager, shortcutManager, storage, sessionId, manifest)

        feature.start()
        verify(session, never()).register(feature)

        feature.stop()
        verify(session, never()).register(feature)
    }

    @Test
    fun `updateStoredManifest is called when the manifest changes`() = runBlockingTest {
        val initialManifest = WebAppManifest(name = "Mozilla", startUrl = "https://mozilla.org")
        val feature = spy(ManifestUpdateFeature(testContext, sessionManager, shortcutManager, storage, sessionId, initialManifest))
        doReturn(Unit).`when`(feature).updateStoredManifest(any())
        feature.start()

        val manifest = WebAppManifest(name = "Mozilla", startUrl = "https://mozilla.org", shortName = "Moz")
        feature.onWebAppManifestChanged(session, manifest)

        verify(feature).updateStoredManifest(manifest)
    }

    @Test
    fun `updateStoredManifest is not called when the manifest is the same`() = runBlockingTest {
        val initialManifest = WebAppManifest(name = "Mozilla", startUrl = "https://mozilla.org")
        val feature = spy(ManifestUpdateFeature(testContext, sessionManager, shortcutManager, storage, sessionId, initialManifest))
        feature.start()

        feature.onWebAppManifestChanged(session, initialManifest)

        verify(feature, never()).updateStoredManifest(initialManifest)
    }

    @Test
    fun `updateStoredManifest is not called when the manifest is null`() = runBlockingTest {
        val initialManifest = WebAppManifest(name = "Mozilla", startUrl = "https://mozilla.org")
        val feature = spy(ManifestUpdateFeature(testContext, sessionManager, shortcutManager, storage, sessionId, initialManifest))
        feature.start()

        feature.onWebAppManifestChanged(session, null)

        verify(feature, never()).updateStoredManifest(any())
    }

    @Test
    fun `updateStoredManifest is not called when the manifest has a different start URL`() = runBlockingTest {
        val initialManifest = WebAppManifest(name = "Mozilla", startUrl = "https://mozilla.org")
        val feature = spy(ManifestUpdateFeature(testContext, sessionManager, shortcutManager, storage, sessionId, initialManifest))
        feature.start()

        val manifest = WebAppManifest(name = "Mozilla", startUrl = "https://netscape.com")
        feature.onWebAppManifestChanged(session, manifest)

        verify(feature, never()).updateStoredManifest(manifest)
    }

    @Test
    fun `updateStoredManifest updates storage and shortcut`() = runBlockingTest {
        val initialManifest = WebAppManifest(name = "Mozilla", startUrl = "https://mozilla.org")
        val feature = ManifestUpdateFeature(testContext, sessionManager, shortcutManager, storage, sessionId, initialManifest)

        val manifest = WebAppManifest(name = "Mozilla", startUrl = "https://mozilla.org", shortName = "Moz")
        feature.updateStoredManifest(manifest)

        verify(storage).updateManifest(manifest)
        verify(shortcutManager).updateShortcuts(testContext, listOf(manifest))
    }

    @Test
    fun `start updates last web app usage`() = runBlockingTest {
        val initialManifest = WebAppManifest(name = "Mozilla", startUrl = "https://mozilla.org", scope = "https://mozilla.org")
        val feature = ManifestUpdateFeature(testContext, sessionManager, shortcutManager, storage, sessionId, initialManifest)

        feature.start()

        verify(storage).updateManifestUsedAt(initialManifest)
    }
}
