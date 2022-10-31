/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs.toolbar

import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.LifecycleRegistry
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.ui.tabcounter.TabCounterMenu
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
@ExperimentalCoroutinesApi
class TabsToolbarFeatureTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private val showTabs: () -> Unit = mock()
    private val tabCounterMenu: TabCounterMenu = mock()
    val toolbar: Toolbar = mock()

    private lateinit var tabsToolbarFeature: TabsToolbarFeature
    private lateinit var lifecycleOwner: MockedLifecycleOwner

    internal class MockedLifecycleOwner(initialState: Lifecycle.State) : LifecycleOwner {
        val lifecycleRegistry = LifecycleRegistry(this).apply {
            currentState = initialState
        }

        override fun getLifecycle(): Lifecycle = lifecycleRegistry
    }

    @Before
    fun setUp() {
        lifecycleOwner = MockedLifecycleOwner(Lifecycle.State.STARTED)
    }

    @Test
    fun `feature adds 'tabs' button to toolbar`() {
        val store = BrowserStore()
        val sessionId: String? = null

        tabsToolbarFeature = TabsToolbarFeature(
            toolbar = toolbar,
            store = store,
            sessionId = sessionId,
            lifecycleOwner = lifecycleOwner,
            showTabs = showTabs,
            tabCounterMenu = tabCounterMenu,
        )

        verify(toolbar).addBrowserAction(any())
    }

    @Test
    fun `feature does not add tabs button when session is a customtab`() {
        val customTabId = "custom-id"
        val customTabSessionState =
            CustomTabSessionState(
                id = customTabId,
                content = ContentState("https://mozilla.org"),
                config = mock(),
            )

        val browserState = BrowserState(customTabs = listOf(customTabSessionState))
        val store = BrowserStore(initialState = browserState)

        tabsToolbarFeature = TabsToolbarFeature(
            toolbar = toolbar,
            store = store,
            sessionId = customTabId,
            lifecycleOwner = lifecycleOwner,
            showTabs = showTabs,
            tabCounterMenu = tabCounterMenu,
        )

        verify(toolbar, never()).addBrowserAction(any())
    }

    @Test
    fun `feature adds tab button when session found but not a customtab`() {
        val tabId = "tab-id"

        val browserState = BrowserState()
        val store = BrowserStore(initialState = browserState)

        tabsToolbarFeature = TabsToolbarFeature(
            toolbar = toolbar,
            store = store,
            sessionId = tabId,
            lifecycleOwner = lifecycleOwner,
            showTabs = showTabs,
            tabCounterMenu = tabCounterMenu,
        )

        verify(toolbar).addBrowserAction(any())
    }
}
