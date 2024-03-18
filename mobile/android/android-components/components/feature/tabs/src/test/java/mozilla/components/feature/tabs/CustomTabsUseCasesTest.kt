/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs

import mozilla.components.browser.state.action.CustomTabListAction
import mozilla.components.browser.state.engine.EngineMiddleware
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.support.test.any
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mockito.ArgumentMatchers.anyBoolean

class CustomTabsUseCasesTest {

    private lateinit var store: BrowserStore
    private lateinit var tabsUseCases: CustomTabsUseCases
    private lateinit var engine: Engine
    private lateinit var engineSession: EngineSession

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Before
    fun setup() {
        engineSession = mock()
        engine = mock()

        whenever(engine.createSession(anyBoolean(), any())).thenReturn(engineSession)
        store = BrowserStore(
            middleware = EngineMiddleware.create(
                engine = engine,
            ),
        )
        tabsUseCases = CustomTabsUseCases(store, mock())
    }

    @Test
    fun `MigrateCustomTabUseCase - turns custom tab into regular tab and selects it`() {
        val customTab = createCustomTab("https://mozilla.org")
        store.dispatch(CustomTabListAction.AddCustomTabAction(customTab)).joinBlocking()
        assertEquals(0, store.state.tabs.size)
        assertEquals(1, store.state.customTabs.size)

        tabsUseCases.migrate(customTab.id, select = false)
        store.waitUntilIdle()

        assertEquals(1, store.state.tabs.size)
        assertEquals(0, store.state.customTabs.size)
        assertNull(store.state.selectedTabId)

        val otherCustomTab = createCustomTab("https://firefox.com")
        store.dispatch(CustomTabListAction.AddCustomTabAction(otherCustomTab)).joinBlocking()
        assertEquals(1, store.state.tabs.size)
        assertEquals(1, store.state.customTabs.size)

        tabsUseCases.migrate(otherCustomTab.id, select = true)
        store.waitUntilIdle()

        assertEquals(2, store.state.tabs.size)
        assertEquals(0, store.state.customTabs.size)
        assertEquals(otherCustomTab.id, store.state.selectedTabId)
    }
}
