/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.setMain
import mozilla.components.browser.session.usecases.EngineSessionUseCases
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.CustomTabListAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineView
import mozilla.components.support.test.any
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import org.junit.After
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Ignore
import org.junit.Test
import org.mockito.ArgumentMatchers
import org.mockito.Mockito.anyString
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.reset
import org.mockito.Mockito.verify

class SessionFeatureTest {
    private val testDispatcher = TestCoroutineDispatcher()

    @Before
    @ExperimentalCoroutinesApi
    fun setUp() {
        Dispatchers.setMain(testDispatcher)
    }

    @After
    @ExperimentalCoroutinesApi
    fun tearDown() {
        Dispatchers.resetMain()
        testDispatcher.cleanupTestCoroutines()
    }

    @Test
    fun `start() renders selected session`() {
        val store = BrowserStore(BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "A"),
                createTab("https://getpocket.com", id = "B"),
                createTab("https://www.firefox.com", id = "C")
            ),
            selectedTabId = "B"
        ))

        val view: EngineView = mock()
        val useCases: EngineSessionUseCases = mock()
        val getOrCreateUseCase: EngineSessionUseCases.GetOrCreateUseCase = mock()
        doReturn(getOrCreateUseCase).`when`(useCases).getOrCreateEngineSession
        val engineSession: EngineSession = mock()
        doReturn(engineSession).`when`(getOrCreateUseCase).invoke(ArgumentMatchers.anyString())

        val feature = SessionFeature(store, mock(), useCases, view)

        verify(getOrCreateUseCase, never()).invoke(anyString())
        verify(view, never()).render(any())

        feature.start()

        testDispatcher.advanceUntilIdle()
        store.waitUntilIdle()

        verify(getOrCreateUseCase).invoke("B")
        verify(view).render(engineSession)
    }

    @Test
    fun `start() renders fixed session`() {
        val store = BrowserStore(BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "A"),
                createTab("https://getpocket.com", id = "B"),
                createTab("https://www.firefox.com", id = "C")
            ),
            selectedTabId = "B"
        ))

        val view: EngineView = mock()
        val useCases: EngineSessionUseCases = mock()
        val getOrCreateUseCase: EngineSessionUseCases.GetOrCreateUseCase = mock()
        doReturn(getOrCreateUseCase).`when`(useCases).getOrCreateEngineSession
        val engineSession: EngineSession = mock()
        doReturn(engineSession).`when`(getOrCreateUseCase).invoke(ArgumentMatchers.anyString())

        val feature = SessionFeature(store, mock(), useCases, view, tabId = "C")

        verify(getOrCreateUseCase, never()).invoke(anyString())
        verify(view, never()).render(any())

        feature.start()

        testDispatcher.advanceUntilIdle()
        store.waitUntilIdle()

        verify(getOrCreateUseCase).invoke("C")
        verify(view).render(engineSession)
    }

    @Test
    fun `start() renders custom tab session`() {
        val store = BrowserStore(BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "A"),
                createTab("https://getpocket.com", id = "B"),
                createTab("https://www.firefox.com", id = "C")
            ),
            customTabs = listOf(
                createCustomTab("https://hubs.mozilla.com/", id = "D")
            ),
            selectedTabId = "B"
        ))

        val view: EngineView = mock()
        val useCases: EngineSessionUseCases = mock()
        val getOrCreateUseCase: EngineSessionUseCases.GetOrCreateUseCase = mock()
        doReturn(getOrCreateUseCase).`when`(useCases).getOrCreateEngineSession
        val engineSession: EngineSession = mock()
        doReturn(engineSession).`when`(getOrCreateUseCase).invoke(ArgumentMatchers.anyString())

        val feature = SessionFeature(store, mock(), useCases, view, tabId = "D")

        verify(getOrCreateUseCase, never()).invoke(anyString())
        verify(view, never()).render(any())

        feature.start()

        testDispatcher.advanceUntilIdle()
        store.waitUntilIdle()

        verify(getOrCreateUseCase).invoke("D")
        verify(view).render(engineSession)
    }

    @Test
    fun `Renders selected tab after changes`() {
        val store = BrowserStore(BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "A"),
                createTab("https://getpocket.com", id = "B"),
                createTab("https://www.firefox.com", id = "C")
            ),
            selectedTabId = "B"
        ))

        val view: EngineView = mock()
        val useCases: EngineSessionUseCases = mock()
        val getOrCreateUseCase: EngineSessionUseCases.GetOrCreateUseCase = mock()
        doReturn(getOrCreateUseCase).`when`(useCases).getOrCreateEngineSession

        val engineSession: EngineSession = mock()
        doReturn(engineSession).`when`(getOrCreateUseCase).invoke(ArgumentMatchers.anyString())

        val feature = SessionFeature(store, mock(), useCases, view)

        verify(getOrCreateUseCase, never()).invoke(anyString())
        verify(view, never()).render(any())

        feature.start()

        testDispatcher.advanceUntilIdle()
        store.waitUntilIdle()

        verify(getOrCreateUseCase).invoke("B")
        verify(view).render(engineSession)

        reset(view)
        reset(getOrCreateUseCase)

        val newEngineSession: EngineSession = mock()
        doReturn(newEngineSession).`when`(getOrCreateUseCase).invoke(ArgumentMatchers.anyString())

        store.dispatch(
            TabListAction.SelectTabAction("A")
        ).joinBlocking()

        verify(getOrCreateUseCase).invoke("A")
        verify(view).render(newEngineSession)
    }

    @Test
    fun `Does not render new selected session after stop()`() {
        val store = BrowserStore(BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "A"),
                createTab("https://getpocket.com", id = "B"),
                createTab("https://www.firefox.com", id = "C")
            ),
            selectedTabId = "B"
        ))

        val view: EngineView = mock()
        val useCases: EngineSessionUseCases = mock()
        val getOrCreateUseCase: EngineSessionUseCases.GetOrCreateUseCase = mock()
        doReturn(getOrCreateUseCase).`when`(useCases).getOrCreateEngineSession

        val engineSession: EngineSession = mock()
        doReturn(engineSession).`when`(getOrCreateUseCase).invoke(ArgumentMatchers.anyString())

        val feature = SessionFeature(store, mock(), useCases, view)

        verify(getOrCreateUseCase, never()).invoke(anyString())
        verify(view, never()).render(any())

        feature.start()

        testDispatcher.advanceUntilIdle()
        store.waitUntilIdle()

        verify(getOrCreateUseCase).invoke("B")
        verify(view).render(engineSession)

        reset(view)
        reset(getOrCreateUseCase)

        val newEngineSession: EngineSession = mock()
        doReturn(newEngineSession).`when`(getOrCreateUseCase).invoke(ArgumentMatchers.anyString())

        feature.stop()

        store.dispatch(
            TabListAction.SelectTabAction("A")
        ).joinBlocking()

        verify(getOrCreateUseCase, never()).invoke("A")
        verify(view, never()).render(newEngineSession)
    }

    @Test
    @Ignore("https://github.com/mozilla-mobile/android-components/issues/7753")
    fun `Releases when last selected session gets removed`() {
        val store = BrowserStore(BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "A")
            ),
            selectedTabId = "A"
        ))

        val view: EngineView = mock()
        val useCases: EngineSessionUseCases = mock()
        val getOrCreateUseCase: EngineSessionUseCases.GetOrCreateUseCase = mock()
        doReturn(getOrCreateUseCase).`when`(useCases).getOrCreateEngineSession
        val engineSession: EngineSession = mock()
        doReturn(engineSession).`when`(getOrCreateUseCase).invoke(ArgumentMatchers.anyString())

        val feature = SessionFeature(store, mock(), useCases, view)

        feature.start()

        testDispatcher.advanceUntilIdle()
        store.waitUntilIdle()

        verify(getOrCreateUseCase).invoke("A")
        verify(view).render(engineSession)
        verify(view, never()).release()

        store.dispatch(
            TabListAction.RemoveTabAction("A")
        ).joinBlocking()

        verify(view).release()
    }

    @Test
    fun `release() stops observing and releases session from view`() {
        val store = BrowserStore(BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "A"),
                createTab("https://getpocket.com", id = "B"),
                createTab("https://www.firefox.com", id = "C")
            ),
            selectedTabId = "B"
        ))

        val view: EngineView = mock()
        val useCases: EngineSessionUseCases = mock()
        val getOrCreateUseCase: EngineSessionUseCases.GetOrCreateUseCase = mock()
        doReturn(getOrCreateUseCase).`when`(useCases).getOrCreateEngineSession

        val engineSession: EngineSession = mock()
        doReturn(engineSession).`when`(getOrCreateUseCase).invoke(ArgumentMatchers.anyString())

        val feature = SessionFeature(store, mock(), useCases, view)

        verify(getOrCreateUseCase, never()).invoke(anyString())
        verify(view, never()).render(any())

        feature.start()

        testDispatcher.advanceUntilIdle()
        store.waitUntilIdle()

        verify(getOrCreateUseCase).invoke("B")
        verify(view).render(engineSession)

        reset(view)
        reset(getOrCreateUseCase)

        val newEngineSession: EngineSession = mock()
        doReturn(newEngineSession).`when`(getOrCreateUseCase).invoke(ArgumentMatchers.anyString())

        feature.release()

        verify(view).release()

        store.dispatch(
            TabListAction.SelectTabAction("A")
        ).joinBlocking()

        verify(getOrCreateUseCase, never()).invoke("A")
        verify(view, never()).render(newEngineSession)
    }

    @Test
    @Ignore("https://github.com/mozilla-mobile/android-components/issues/7753")
    fun `Releases when custom tab gets removed`() {
        val store = BrowserStore(BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "A"),
                createTab("https://getpocket.com", id = "B"),
                createTab("https://www.firefox.com", id = "C")
            ),
            customTabs = listOf(
                createCustomTab("https://hubs.mozilla.com/", id = "D")
            ),
            selectedTabId = "B"
        ))

        val view: EngineView = mock()
        val useCases: EngineSessionUseCases = mock()
        val getOrCreateUseCase: EngineSessionUseCases.GetOrCreateUseCase = mock()
        doReturn(getOrCreateUseCase).`when`(useCases).getOrCreateEngineSession
        val engineSession: EngineSession = mock()
        doReturn(engineSession).`when`(getOrCreateUseCase).invoke(ArgumentMatchers.anyString())

        val feature = SessionFeature(store, mock(), useCases, view, tabId = "D")

        verify(getOrCreateUseCase, never()).invoke(anyString())
        verify(view, never()).render(any())

        feature.start()

        testDispatcher.advanceUntilIdle()
        store.waitUntilIdle()

        verify(getOrCreateUseCase).invoke("D")
        verify(view).render(engineSession)
        verify(view, never()).release()

        store.dispatch(
            CustomTabListAction.RemoveCustomTabAction("D")
        ).joinBlocking()

        verify(view).release()
    }

    @Test
    fun `onBackPressed() clears selection if it exists`() {
        run {
            val view: EngineView = mock()
            doReturn(false).`when`(view).canClearSelection()

            val feature = SessionFeature(BrowserStore(), mock(), mock(), view)
            assertFalse(feature.onBackPressed())

            verify(view, never()).clearSelection()
        }

        run {
            val view: EngineView = mock()
            doReturn(true).`when`(view).canClearSelection()

            val feature = SessionFeature(BrowserStore(), mock(), mock(), view)
            assertTrue(feature.onBackPressed())

            verify(view).clearSelection()
        }
    }

    @Test
    fun `onBackPressed() invokes GoBackUseCase if back navigation is possible`() {
        run {
            val store = BrowserStore(BrowserState(
                tabs = listOf(createTab("https://www.mozilla.org", id = "A")),
                selectedTabId = "A"
            ))

            val useCase: SessionUseCases.GoBackUseCase = mock()

            val feature = SessionFeature(store, useCase, mock(), mock())

            assertFalse(feature.onBackPressed())
            verify(useCase, never()).invoke("A")
        }

        run {
            val store = BrowserStore(BrowserState(
                tabs = listOf(createTab("https://www.mozilla.org", id = "A")),
                selectedTabId = "A"
            ))

            store.dispatch(ContentAction.UpdateBackNavigationStateAction(
                "A",
                canGoBack = true
            )).joinBlocking()

            val useCase: SessionUseCases.GoBackUseCase = mock()

            val feature = SessionFeature(store, useCase, mock(), mock())

            assertTrue(feature.onBackPressed())
            verify(useCase).invoke("A")
        }
    }
}
