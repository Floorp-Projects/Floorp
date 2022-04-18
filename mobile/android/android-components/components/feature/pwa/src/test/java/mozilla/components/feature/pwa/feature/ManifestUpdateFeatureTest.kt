/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.feature

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.feature.pwa.ManifestStorage
import mozilla.components.feature.pwa.WebAppShortcutManager
import mozilla.components.support.test.any
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class ManifestUpdateFeatureTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private lateinit var shortcutManager: WebAppShortcutManager
    private lateinit var storage: ManifestStorage
    private lateinit var store: BrowserStore

    private val sessionId = "external-app-session-id"
    private val baseManifest = WebAppManifest(
        name = "Mozilla",
        startUrl = "https://mozilla.org",
        scope = "https://mozilla.org"
    )

    @Before
    fun setUp() {
        storage = mock()
        shortcutManager = mock()

        store = BrowserStore(
            BrowserState(
                customTabs = listOf(
                    createCustomTab("https://mozilla.org", id = sessionId)
                )
            )
        )
    }

    @Test
    fun `start and stop handle null session`() = runBlockingTest {
        val feature = ManifestUpdateFeature(
            testContext,
            store,
            shortcutManager,
            storage,
            "not existing",
            baseManifest
        )

        feature.start()

        store.waitUntilIdle()

        feature.stop()

        verify(storage).updateManifestUsedAt(baseManifest)
        verify(storage, never()).updateManifest(any())
    }

    @Test
    fun `Last usage is updated when feature is started`() {
        val feature = ManifestUpdateFeature(
            testContext,
            store,
            shortcutManager,
            storage,
            sessionId,
            baseManifest
        )

        // Insert base manifest
        store.dispatch(
            ContentAction.UpdateWebAppManifestAction(
                sessionId,
                baseManifest
            )
        ).joinBlocking()

        feature.start()

        feature.updateUsageJob!!.joinBlocking()

        runBlocking {
            verify(storage).updateManifestUsedAt(baseManifest)
        }
    }

    @Test
    fun `updateStoredManifest is called when the manifest changes`() {
        val feature = ManifestUpdateFeature(
            testContext,
            store,
            shortcutManager,
            storage,
            sessionId,
            baseManifest
        )

        // Insert base manifest
        store.dispatch(
            ContentAction.UpdateWebAppManifestAction(
                sessionId,
                baseManifest
            )
        ).joinBlocking()

        feature.start()

        val newManifest = baseManifest.copy(shortName = "Moz")

        // Update manifest
        store.dispatch(
            ContentAction.UpdateWebAppManifestAction(
                sessionId,
                newManifest
            )
        ).joinBlocking()

        feature.updateJob!!.joinBlocking()

        runBlocking {
            verify(storage).updateManifest(newManifest)
        }
    }

    @Test
    fun `updateStoredManifest is not called when the manifest is the same`() {
        val feature = ManifestUpdateFeature(
            testContext,
            store,
            shortcutManager,
            storage,
            sessionId,
            baseManifest
        )

        feature.start()

        // Update manifest
        store.dispatch(
            ContentAction.UpdateWebAppManifestAction(
                sessionId,
                baseManifest
            )
        ).joinBlocking()

        feature.updateJob?.joinBlocking()

        runBlocking {
            verify(storage, never()).updateManifest(any())
        }
    }

    @Test
    fun `updateStoredManifest is not called when the manifest is removed`() {
        val feature = ManifestUpdateFeature(
            testContext,
            store,
            shortcutManager,
            storage,
            sessionId,
            baseManifest
        )

        // Insert base manifest
        store.dispatch(
            ContentAction.UpdateWebAppManifestAction(
                sessionId,
                baseManifest
            )
        ).joinBlocking()

        feature.start()

        // Update manifest
        store.dispatch(
            ContentAction.RemoveWebAppManifestAction(
                sessionId
            )
        ).joinBlocking()

        feature.updateJob?.joinBlocking()

        runBlocking {
            verify(storage, never()).updateManifest(any())
        }
    }

    @Test
    fun `updateStoredManifest is not called when the manifest has a different start URL`() {
        val feature = ManifestUpdateFeature(
            testContext,
            store,
            shortcutManager,
            storage,
            sessionId,
            baseManifest
        )

        // Insert base manifest
        store.dispatch(
            ContentAction.UpdateWebAppManifestAction(
                sessionId,
                baseManifest
            )
        ).joinBlocking()

        feature.start()

        // Update manifest
        store.dispatch(
            ContentAction.UpdateWebAppManifestAction(
                sessionId,
                WebAppManifest(name = "Mozilla", startUrl = "https://netscape.com")
            )
        ).joinBlocking()

        feature.updateJob?.joinBlocking()

        runBlocking {
            verify(storage, never()).updateManifest(any())
        }
    }

    @Test
    fun `updateStoredManifest updates storage and shortcut`() = runBlockingTest {
        val feature = ManifestUpdateFeature(testContext, store, shortcutManager, storage, sessionId, baseManifest)

        val manifest = baseManifest.copy(shortName = "Moz")
        feature.updateStoredManifest(manifest)

        verify(storage).updateManifest(manifest)
        verify(shortcutManager).updateShortcuts(testContext, listOf(manifest))
    }

    @Test
    fun `start updates last web app usage`() = runBlockingTest {
        val feature = ManifestUpdateFeature(testContext, store, shortcutManager, storage, sessionId, baseManifest)

        feature.start()

        verify(storage).updateManifestUsedAt(baseManifest)
    }
}
