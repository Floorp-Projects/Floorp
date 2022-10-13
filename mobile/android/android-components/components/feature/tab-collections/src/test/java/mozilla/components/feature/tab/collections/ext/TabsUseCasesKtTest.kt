/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tab.collections.ext

import mozilla.components.browser.state.engine.EngineMiddleware
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.state.recover.toRecoverableTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.feature.tab.collections.Tab
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.whenever
import org.junit.Assert.assertNotEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.any
import org.mockito.ArgumentMatchers.anyBoolean
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class TabsUseCasesKtTest {

    private lateinit var store: BrowserStore
    private lateinit var tabsUseCases: TabsUseCases
    private lateinit var engine: Engine
    private lateinit var engineSession: EngineSession

    private lateinit var collection: TabCollection
    private lateinit var tab: Tab

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
        tabsUseCases = TabsUseCases(store)

        val recoveredTab = createTab(
            id = "123",
            url = "https://mozilla.org",
            lastAccess = 3735928559L,
        ).toRecoverableTab()

        tab = mock<Tab>().apply {
            whenever(id).thenReturn(123)
            whenever(title).thenReturn("Firefox")
            whenever(url).thenReturn("https://firefox.com")
            whenever(restore(testContext, engine, false)).thenReturn(recoveredTab)
        }
        collection = mock<TabCollection>().apply {
            whenever(tabs).thenReturn(listOf(tab))
        }
    }

    @Test
    fun `RestoreUseCase updates last access when restoring collection`() {
        tabsUseCases.restore.invoke(testContext, engine, collection) {}

        store.waitUntilIdle()

        assertNotEquals(3735928559L, store.state.findTab("123")!!.lastAccess)
    }

    @Test
    fun `RestoreUseCase updates last access when restoring single tab in collection`() {
        tabsUseCases.restore.invoke(testContext, engine, tab, onTabRestored = {}, onFailure = {})

        store.waitUntilIdle()

        assertNotEquals(3735928559L, store.state.findTab("123")!!.lastAccess)
    }
}
