/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.setMain
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.window.WindowRequest
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class WindowFeatureTest {

    private val testDispatcher = TestCoroutineDispatcher()

    private lateinit var store: BrowserStore
    private lateinit var tabsUseCases: TabsUseCases
    private lateinit var addTabUseCase: TabsUseCases.AddNewTabUseCase
    private lateinit var addPrivateTabUseCase: TabsUseCases.AddNewPrivateTabUseCase
    private lateinit var removeTabUseCase: TabsUseCases.RemoveTabUseCase
    private val tabId = "test-tab"
    private val privateTabId = "test-tab-private"

    @Before
    fun setup() {
        Dispatchers.setMain(testDispatcher)
        store = spy(BrowserStore(BrowserState(
            tabs = listOf(
                createTab(id = tabId, url = "https://www.mozilla.org"),
                createTab(id = privateTabId, url = "https://www.mozilla.org", private = true)
            ),
            selectedTabId = tabId
        )))
        addPrivateTabUseCase = mock()
        addTabUseCase = mock()
        removeTabUseCase = mock()
        tabsUseCases = mock()
        whenever(tabsUseCases.addTab).thenReturn(addTabUseCase)
        whenever(tabsUseCases.addPrivateTab).thenReturn(addPrivateTabUseCase)
        whenever(tabsUseCases.removeTab).thenReturn(removeTabUseCase)
    }

    @After
    @ExperimentalCoroutinesApi
    fun tearDown() {
        Dispatchers.resetMain()
        testDispatcher.cleanupTestCoroutines()
    }

    @Test
    fun `handles request to open window`() {
        val feature = WindowFeature(store, tabsUseCases)
        feature.start()

        val windowRequest: WindowRequest = mock()
        whenever(windowRequest.type).thenReturn(WindowRequest.Type.OPEN)
        whenever(windowRequest.url).thenReturn("https://www.firefox.com")

        store.dispatch(ContentAction.UpdateWindowRequestAction(tabId, windowRequest)).joinBlocking()
        testDispatcher.advanceUntilIdle()
        verify(addTabUseCase).invoke(url = windowRequest.url, selectTab = true, parentId = tabId)
        verify(store).dispatch(ContentAction.ConsumeWindowRequestAction(tabId))
    }

    @Test
    fun `handles request to open private window`() {
        val feature = WindowFeature(store, tabsUseCases)
        feature.start()

        val windowRequest: WindowRequest = mock()
        whenever(windowRequest.type).thenReturn(WindowRequest.Type.OPEN)
        whenever(windowRequest.url).thenReturn("https://www.firefox.com")

        store.dispatch(TabListAction.SelectTabAction(privateTabId)).joinBlocking()
        store.dispatch(ContentAction.UpdateWindowRequestAction(privateTabId, windowRequest)).joinBlocking()
        testDispatcher.advanceUntilIdle()
        verify(addPrivateTabUseCase).invoke(url = windowRequest.url, selectTab = true, parentId = privateTabId)
        verify(store).dispatch(ContentAction.ConsumeWindowRequestAction(privateTabId))
    }

    @Test
    fun `handles request to close window`() {
        val feature = WindowFeature(store, tabsUseCases)
        feature.start()

        val engineSession: EngineSession = mock()
        store.dispatch(EngineAction.LinkEngineSessionAction(tabId, engineSession))

        val windowRequest: WindowRequest = mock()
        whenever(windowRequest.type).thenReturn(WindowRequest.Type.CLOSE)
        whenever(windowRequest.prepare()).thenReturn(engineSession)

        store.dispatch(ContentAction.UpdateWindowRequestAction(tabId, windowRequest)).joinBlocking()
        testDispatcher.advanceUntilIdle()
        verify(removeTabUseCase).invoke(tabId)
        verify(store).dispatch(ContentAction.ConsumeWindowRequestAction(tabId))
    }

    @Test
    fun `handles no requests when stopped`() {
        val feature = WindowFeature(store, tabsUseCases)
        feature.start()
        feature.stop()

        val windowRequest: WindowRequest = mock()
        whenever(windowRequest.type).thenReturn(WindowRequest.Type.CLOSE)

        store.dispatch(ContentAction.UpdateWindowRequestAction(tabId, windowRequest)).joinBlocking()
        testDispatcher.advanceUntilIdle()
        verify(removeTabUseCase, never()).invoke(tabId)
        verify(store, never()).dispatch(ContentAction.ConsumeWindowRequestAction(tabId))
    }
}