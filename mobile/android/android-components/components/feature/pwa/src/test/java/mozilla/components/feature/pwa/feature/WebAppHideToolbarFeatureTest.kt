/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.feature

import androidx.browser.customtabs.CustomTabsService.RELATION_HANDLE_ALL_URLS
import androidx.browser.customtabs.CustomTabsSessionToken
import androidx.core.net.toUri
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.CustomTabConfig
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.feature.customtabs.store.CustomTabState
import mozilla.components.feature.customtabs.store.CustomTabsServiceState
import mozilla.components.feature.customtabs.store.CustomTabsServiceStore
import mozilla.components.feature.customtabs.store.OriginRelationPair
import mozilla.components.feature.customtabs.store.ValidateRelationshipAction
import mozilla.components.feature.customtabs.store.VerificationStatus
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class WebAppHideToolbarFeatureTest {

    private val customTabId = "custom-id"
    private var toolbarVisible = false

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Before
    fun setup() {
        toolbarVisible = false
    }

    @Test
    fun `hides toolbar immediately based on PWA manifest`() {
        val tab = CustomTabSessionState(
            id = customTabId,
            content = ContentState("https://mozilla.org"),
            config = CustomTabConfig(),
        )
        val store = BrowserStore(BrowserState(customTabs = listOf(tab)))

        val feature = WebAppHideToolbarFeature(
            store,
            CustomTabsServiceStore(),
            tabId = tab.id,
            manifest = mockManifest("https://mozilla.org"),
        ) {
            toolbarVisible = it
        }
        feature.start()
        assertFalse(toolbarVisible)
    }

    @Test
    fun `hides toolbar immediately based on trusted origins`() {
        val token = mock<CustomTabsSessionToken>()
        val tab = CustomTabSessionState(
            id = customTabId,
            content = ContentState("https://mozilla.org"),
            config = CustomTabConfig(sessionToken = token),
        )
        val store = BrowserStore(BrowserState(customTabs = listOf(tab)))
        val customTabsStore = CustomTabsServiceStore(
            CustomTabsServiceState(
                tabs = mapOf(token to mockCustomTabState("https://firefox.com", "https://mozilla.org")),
            ),
        )

        val feature = WebAppHideToolbarFeature(
            store,
            customTabsStore,
            tabId = tab.id,
        ) {
            toolbarVisible = it
        }
        feature.start()
        assertFalse(toolbarVisible)
    }

    @Test
    fun `does not hide toolbar for a normal tab`() {
        val tab = createTab("https://mozilla.org")
        val store = BrowserStore(BrowserState(tabs = listOf(tab)))

        val feature = WebAppHideToolbarFeature(store, CustomTabsServiceStore(), tabId = tab.id) {
            toolbarVisible = it
        }
        feature.start()
        assertTrue(toolbarVisible)
    }

    @Test
    fun `does not hide toolbar for an invalid tab`() {
        val store = BrowserStore()

        val feature = WebAppHideToolbarFeature(store, CustomTabsServiceStore()) {
            toolbarVisible = it
        }
        feature.start()
        assertTrue(toolbarVisible)
    }

    @Test
    fun `does hide toolbar for a normal tab in fullscreen`() {
        val tab = TabSessionState(
            content = ContentState(
                url = "https://mozilla.org",
                fullScreen = true,
            ),
        )
        val store = BrowserStore(BrowserState(tabs = listOf(tab)))

        val feature = WebAppHideToolbarFeature(store, CustomTabsServiceStore(), tabId = tab.id) {
            toolbarVisible = it
        }
        feature.start()
        assertFalse(toolbarVisible)
    }

    @Test
    fun `does hide toolbar for a normal tab in PIP`() {
        val tab = TabSessionState(
            content = ContentState(
                url = "https://mozilla.org",
                pictureInPictureEnabled = true,
            ),
        )
        val store = BrowserStore(BrowserState(tabs = listOf(tab)))

        val feature = WebAppHideToolbarFeature(store, CustomTabsServiceStore(), tabId = tab.id) {
            toolbarVisible = it
        }
        feature.start()
        assertFalse(toolbarVisible)
    }

    @Test
    fun `does not hide toolbar if origin is not trusted`() {
        val token = mock<CustomTabsSessionToken>()
        val tab = createCustomTab(
            id = customTabId,
            url = "https://firefox.com",
            config = CustomTabConfig(sessionToken = token),
        )
        val store = BrowserStore(BrowserState(customTabs = listOf(tab)))
        val customTabsStore = CustomTabsServiceStore(
            CustomTabsServiceState(
                tabs = mapOf(token to mockCustomTabState("https://mozilla.org")),
            ),
        )

        val feature = WebAppHideToolbarFeature(
            store,
            customTabsStore,
            tabId = tab.id,
        ) {
            toolbarVisible = it
        }
        feature.start()
        assertTrue(toolbarVisible)
    }

    @Test
    fun `onUrlChanged hides toolbar if URL is in origin`() {
        val token = mock<CustomTabsSessionToken>()
        val tab = createCustomTab(
            id = customTabId,
            url = "https://mozilla.org",
            config = CustomTabConfig(sessionToken = token),
        )
        val store = BrowserStore(BrowserState(customTabs = listOf(tab)))
        val customTabsStore = CustomTabsServiceStore(
            CustomTabsServiceState(
                tabs = mapOf(token to mockCustomTabState("https://mozilla.com", "https://m.mozilla.com")),
            ),
        )
        val feature = WebAppHideToolbarFeature(
            store,
            customTabsStore,
            tabId = customTabId,
        ) {
            toolbarVisible = it
        }
        feature.start()

        store.dispatch(
            ContentAction.UpdateUrlAction(customTabId, "https://mozilla.com/example-page"),
        ).joinBlocking()
        assertFalse(toolbarVisible)

        store.dispatch(
            ContentAction.UpdateUrlAction(customTabId, "https://firefox.com/out-of-scope"),
        ).joinBlocking()
        assertTrue(toolbarVisible)

        store.dispatch(
            ContentAction.UpdateUrlAction(customTabId, "https://mozilla.com/back-in-scope"),
        ).joinBlocking()
        assertFalse(toolbarVisible)

        store.dispatch(
            ContentAction.UpdateUrlAction(customTabId, "https://m.mozilla.com/second-origin"),
        ).joinBlocking()
        assertFalse(toolbarVisible)
    }

    @Test
    fun `onUrlChanged hides toolbar if URL is in scope`() {
        val tab = createCustomTab(id = customTabId, url = "https://mozilla.org")
        val store = BrowserStore(BrowserState(customTabs = listOf(tab)))
        val feature = WebAppHideToolbarFeature(
            store,
            CustomTabsServiceStore(),
            tabId = customTabId,
            manifest = mockManifest("https://mozilla.github.io/my-app/"),
        ) {
            toolbarVisible = it
        }
        feature.start()

        store.dispatch(
            ContentAction.UpdateUrlAction(customTabId, "https://mozilla.github.io/my-app/"),
        ).joinBlocking()
        assertFalse(toolbarVisible)

        store.dispatch(
            ContentAction.UpdateUrlAction(customTabId, "https://firefox.com/out-of-scope"),
        ).joinBlocking()
        assertTrue(toolbarVisible)

        store.dispatch(
            ContentAction.UpdateUrlAction(customTabId, "https://mozilla.github.io/my-app-almost-in-scope"),
        ).joinBlocking()
        assertTrue(toolbarVisible)

        store.dispatch(
            ContentAction.UpdateUrlAction(customTabId, "https://mozilla.github.io/my-app/sub-page"),
        ).joinBlocking()
        assertFalse(toolbarVisible)
    }

    @Test
    fun `onUrlChanged hides toolbar if URL is in ambiguous scope`() {
        val tab = createCustomTab(id = customTabId, url = "https://mozilla.org")
        val store = BrowserStore(BrowserState(customTabs = listOf(tab)))
        val feature = WebAppHideToolbarFeature(
            store,
            CustomTabsServiceStore(),
            tabId = customTabId,
            manifest = mockManifest("https://mozilla.github.io/prefix"),
        ) {
            toolbarVisible = it
        }
        feature.start()

        store.dispatch(
            ContentAction.UpdateUrlAction(customTabId, "https://mozilla.github.io/prefix/"),
        ).joinBlocking()
        assertFalse(toolbarVisible)

        store.dispatch(
            ContentAction.UpdateUrlAction(customTabId, "https://mozilla.github.io/prefix-of/resource.html"),
        ).joinBlocking()
        assertFalse(toolbarVisible)
    }

    @Test
    fun `onTrustedScopesChange hides toolbar if URL is in origin`() {
        val token = mock<CustomTabsSessionToken>()
        val tab = createCustomTab(
            id = customTabId,
            url = "https://mozilla.com/example-page",
            config = CustomTabConfig(sessionToken = token),
        )
        val store = BrowserStore(BrowserState(customTabs = listOf(tab)))
        val customTabsStore = CustomTabsServiceStore(
            CustomTabsServiceState(
                tabs = mapOf(token to mockCustomTabState()),
            ),
        )
        val feature = WebAppHideToolbarFeature(
            store,
            customTabsStore,
            tabId = customTabId,
        ) {
            toolbarVisible = it
        }
        feature.start()

        customTabsStore.dispatch(
            ValidateRelationshipAction(
                token,
                RELATION_HANDLE_ALL_URLS,
                "https://m.mozilla.com".toUri(),
                VerificationStatus.PENDING,
            ),
        ).joinBlocking()
        assertTrue(toolbarVisible)

        customTabsStore.dispatch(
            ValidateRelationshipAction(
                token,
                RELATION_HANDLE_ALL_URLS,
                "https://mozilla.com".toUri(),
                VerificationStatus.PENDING,
            ),
        ).joinBlocking()
        assertFalse(toolbarVisible)
    }

    private fun mockCustomTabState(vararg origins: String) = CustomTabState(
        relationships = origins.map { origin ->
            OriginRelationPair(origin.toUri(), RELATION_HANDLE_ALL_URLS) to VerificationStatus.PENDING
        }.toMap(),
    )

    private fun mockManifest(scope: String) = WebAppManifest(
        name = "Mock",
        startUrl = scope,
        scope = scope,
        display = WebAppManifest.DisplayMode.STANDALONE,
    )
}
