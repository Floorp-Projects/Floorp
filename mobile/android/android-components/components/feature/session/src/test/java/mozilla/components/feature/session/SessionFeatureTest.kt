/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import android.view.View
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.CrashAction
import mozilla.components.browser.state.action.CustomTabListAction
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.engine.EngineMiddleware
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineView
import mozilla.components.support.test.any
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.middleware.CaptureActionsMiddleware
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.mockito.Mockito.atLeastOnce
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

class SessionFeatureTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val scope = coroutinesTestRule.scope

    @Test
    fun `start renders selected session`() {
        val store = prepareStore()

        val actualView: View = mock()
        val view: EngineView = mock()
        doReturn(actualView).`when`(view).asView()

        val engineSession: EngineSession = mock()
        store.dispatch(EngineAction.LinkEngineSessionAction("B", engineSession)).joinBlocking()

        val feature = SessionFeature(store, mock(), view)
        verify(view, never()).render(any())

        feature.start()

        store.waitUntilIdle()
        verify(view).render(engineSession)
    }

    @Test
    fun `start renders fixed session`() {
        val store = prepareStore()

        val actualView: View = mock()
        val view: EngineView = mock()
        doReturn(actualView).`when`(view).asView()

        val engineSession: EngineSession = mock()
        store.dispatch(EngineAction.LinkEngineSessionAction("C", engineSession)).joinBlocking()

        val feature = SessionFeature(store, mock(), view, tabId = "C")
        verify(view, never()).render(any())

        feature.start()

        store.waitUntilIdle()

        verify(view).render(engineSession)
    }

    @Test
    fun `start renders custom tab session`() {
        val store = prepareStore()

        val actualView: View = mock()
        val view: EngineView = mock()
        doReturn(actualView).`when`(view).asView()

        val engineSession: EngineSession = mock()
        store.dispatch(EngineAction.LinkEngineSessionAction("D", engineSession)).joinBlocking()

        val feature = SessionFeature(store, mock(), view, tabId = "D")
        verify(view, never()).render(any())
        feature.start()

        store.waitUntilIdle()

        verify(view).render(engineSession)
    }

    @Test
    fun `renders selected tab after changes`() {
        val store = prepareStore()

        val actualView: View = mock()
        val view: EngineView = mock()
        doReturn(actualView).`when`(view).asView()

        val engineSessionA: EngineSession = mock()
        val engineSessionB: EngineSession = mock()
        store.dispatch(EngineAction.LinkEngineSessionAction("A", engineSessionA)).joinBlocking()
        store.dispatch(EngineAction.LinkEngineSessionAction("B", engineSessionB)).joinBlocking()

        val feature = SessionFeature(store, mock(), view)
        verify(view, never()).render(any())

        feature.start()
        store.waitUntilIdle()
        verify(view).render(engineSessionB)

        store.dispatch(TabListAction.SelectTabAction("A")).joinBlocking()
        store.waitUntilIdle()
        verify(view).render(engineSessionA)
    }

    @Test
    fun `creates engine session if needed`() {
        val store = spy(prepareStore())
        val actualView: View = mock()
        val view: EngineView = mock()
        doReturn(actualView).`when`(view).asView()

        val feature = SessionFeature(store, mock(), view)
        verify(view, never()).render(any())

        feature.start()
        store.waitUntilIdle()
        verify(store).dispatch(EngineAction.CreateEngineSessionAction("B"))
    }

    @Test
    fun `does not render new selected session after stop`() {
        val store = prepareStore()

        val actualView: View = mock()
        val view: EngineView = mock()
        doReturn(actualView).`when`(view).asView()

        val engineSessionA: EngineSession = mock()
        val engineSessionB: EngineSession = mock()
        store.dispatch(EngineAction.LinkEngineSessionAction("A", engineSessionA)).joinBlocking()
        store.dispatch(EngineAction.LinkEngineSessionAction("B", engineSessionB)).joinBlocking()

        val feature = SessionFeature(store, mock(), view)
        verify(view, never()).render(any())

        feature.start()
        store.waitUntilIdle()
        verify(view).render(engineSessionB)

        feature.stop()

        store.dispatch(TabListAction.SelectTabAction("A")).joinBlocking()
        store.waitUntilIdle()
        verify(view, never()).render(engineSessionA)
    }

    @Test
    fun `releases when last selected session gets removed`() {
        val store = prepareStore()

        val actualView: View = mock()
        val view: EngineView = mock()
        doReturn(actualView).`when`(view).asView()

        val engineSession: EngineSession = mock()
        store.dispatch(EngineAction.LinkEngineSessionAction("B", engineSession)).joinBlocking()
        val feature = SessionFeature(store, mock(), view)

        feature.start()

        store.waitUntilIdle()

        verify(view).render(engineSession)
        verify(view, never()).release()

        store.dispatch(TabListAction.RemoveAllTabsAction()).joinBlocking()
        verify(view).release()
    }

    @Test
    fun `release stops observing and releases session from view`() {
        val store = prepareStore()
        val actualView: View = mock()

        val view: EngineView = mock()
        doReturn(actualView).`when`(view).asView()

        val engineSession: EngineSession = mock()
        store.dispatch(EngineAction.LinkEngineSessionAction("B", engineSession)).joinBlocking()

        val feature = SessionFeature(store, mock(), view)
        verify(view, never()).render(any())

        feature.start()

        store.waitUntilIdle()

        verify(view).render(engineSession)

        val newEngineSession: EngineSession = mock()
        feature.release()
        verify(view).release()

        store.dispatch(TabListAction.SelectTabAction("A")).joinBlocking()
        verify(view, never()).render(newEngineSession)
    }

    @Test
    fun `releases when custom tab gets removed`() {
        val store = prepareStore()

        val actualView: View = mock()
        val view: EngineView = mock()
        doReturn(actualView).`when`(view).asView()

        val engineSession: EngineSession = mock()
        store.dispatch(EngineAction.LinkEngineSessionAction("D", engineSession)).joinBlocking()

        val feature = SessionFeature(store, mock(), view, tabId = "D")
        verify(view, never()).render(any())

        feature.start()

        store.waitUntilIdle()

        verify(view).render(engineSession)
        verify(view, never()).release()

        store.dispatch(CustomTabListAction.RemoveCustomTabAction("D")).joinBlocking()
        verify(view).release()
    }

    @Test
    fun `onBackPressed clears selection if it exists`() {
        run {
            val view: EngineView = mock()
            doReturn(false).`when`(view).canClearSelection()

            val feature = SessionFeature(BrowserStore(), mock(), view)
            assertFalse(feature.onBackPressed())

            verify(view, never()).clearSelection()
        }

        run {
            val view: EngineView = mock()
            doReturn(true).`when`(view).canClearSelection()

            val feature = SessionFeature(BrowserStore(), mock(), view)
            assertTrue(feature.onBackPressed())

            verify(view).clearSelection()
        }
    }

    @Test
    fun `onBackPressed() invokes GoBackUseCase if back navigation is possible`() {
        run {
            val store = BrowserStore(
                BrowserState(
                    tabs = listOf(createTab("https://www.mozilla.org", id = "A")),
                    selectedTabId = "A",
                ),
            )

            val useCase: SessionUseCases.GoBackUseCase = mock()

            val feature = SessionFeature(store, useCase, mock())

            assertFalse(feature.onBackPressed())
            verify(useCase, never()).invoke("A")
        }

        run {
            val store = BrowserStore(
                BrowserState(
                    tabs = listOf(createTab("https://www.mozilla.org", id = "A")),
                    selectedTabId = "A",
                ),
            )

            store.dispatch(
                ContentAction.UpdateBackNavigationStateAction(
                    "A",
                    canGoBack = true,
                ),
            ).joinBlocking()

            val useCase: SessionUseCases.GoBackUseCase = mock()

            val feature = SessionFeature(store, useCase, mock())

            assertTrue(feature.onBackPressed())
            verify(useCase).invoke("A")
        }
    }

    @Test
    fun `stop releases engine view`() {
        val store = prepareStore()

        val actualView: View = mock()
        val view: EngineView = mock()
        doReturn(actualView).`when`(view).asView()

        val engineSession: EngineSession = mock()
        store.dispatch(EngineAction.LinkEngineSessionAction("D", engineSession)).joinBlocking()

        val feature = SessionFeature(store, mock(), view, tabId = "D")
        verify(view, never()).render(any())
        feature.start()

        store.waitUntilIdle()

        verify(view).render(engineSession)

        feature.stop()
        verify(view).release()
    }

    @Test
    fun `presenter observes crash state and does not create new engine session immediately`() {
        val middleware = CaptureActionsMiddleware<BrowserState, BrowserAction>()
        val store = prepareStore(middleware)

        val actualView: View = mock()
        val view: EngineView = mock()
        doReturn(actualView).`when`(view).asView()

        val engineSession: EngineSession = mock()
        store.dispatch(EngineAction.LinkEngineSessionAction("A", engineSession)).joinBlocking()

        val feature = SessionFeature(store, mock(), view, tabId = "A")
        verify(view, never()).render(any())
        feature.start()

        store.dispatch(CrashAction.SessionCrashedAction("A")).joinBlocking()
        store.waitUntilIdle()
        verify(view, atLeastOnce()).release()
        middleware.assertNotDispatched(EngineAction.CreateEngineSessionAction::class)
    }

    @Test
    fun `last access is updated when session is rendered`() {
        val store = prepareStore()

        val actualView: View = mock()
        val view: EngineView = mock()
        doReturn(actualView).`when`(view).asView()

        val engineSession: EngineSession = mock()
        store.dispatch(EngineAction.LinkEngineSessionAction("B", engineSession)).joinBlocking()

        val feature = SessionFeature(store, mock(), view)
        verify(view, never()).render(any())

        assertEquals(0L, store.state.findTab("B")?.lastAccess)
        feature.start()
        store.waitUntilIdle()

        assertNotEquals(0L, store.state.findTab("B")?.lastAccess)
        verify(view).render(engineSession)
    }

    private fun prepareStore(
        middleware: CaptureActionsMiddleware<BrowserState, BrowserAction>? = null,
    ): BrowserStore = BrowserStore(
        BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "A"),
                createTab("https://getpocket.com", id = "B"),
                createTab("https://www.firefox.com", id = "C"),
            ),
            customTabs = listOf(
                createCustomTab("https://hubs.mozilla.com/", id = "D"),
            ),
            selectedTabId = "B",
        ),
        middleware = (if (middleware != null) listOf(middleware) else emptyList()) + EngineMiddleware.create(
            engine = mock(),
            scope = scope,
        ),
    )
}
