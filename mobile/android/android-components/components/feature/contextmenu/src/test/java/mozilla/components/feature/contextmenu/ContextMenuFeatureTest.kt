/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.contextmenu

import android.view.HapticFeedbackConstants
import android.view.View
import androidx.fragment.app.FragmentManager
import androidx.fragment.app.FragmentTransaction
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.setMain
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.engine.HitResult
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.processor.CollectionProcessor
import mozilla.components.support.base.observer.Consumable
import mozilla.components.support.test.any
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class ContextMenuFeatureTest {
    private val testDispatcher = TestCoroutineDispatcher()

    private lateinit var store: BrowserStore

    @Before
    fun setUp() {
        Dispatchers.setMain(testDispatcher)

        store = BrowserStore(BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "test-tab")
            ),
            selectedTabId = "test-tab"
        ))
    }

    @After
    fun tearDown() {
        Dispatchers.resetMain()
        testDispatcher.cleanupTestCoroutines()
    }

    @Test
    fun `New HitResult for selected session will cause fragment transaction`() {
        val fragmentManager = mockFragmentManager()

        val (engineView, view) = mockEngineView()

        val feature = ContextMenuFeature(
            fragmentManager,
            store,
            ContextMenuCandidate.defaultCandidates(testContext, mock(), mock(), mock()),
            engineView,
            mock())

        feature.start()

        store.dispatch(ContentAction.UpdateHitResultAction(
            "test-tab",
            HitResult.UNKNOWN("https://www.mozilla.org")
        )).joinBlocking()

        testDispatcher.advanceUntilIdle()

        verify(fragmentManager).beginTransaction()
        verify(view).performHapticFeedback(HapticFeedbackConstants.LONG_PRESS)
    }

    @Test
    fun `New HitResult for selected session will not cause fragment transaction if feature is stopped`() {
        val fragmentManager = mockFragmentManager()

        val (engineView, view) = mockEngineView()

        val feature = ContextMenuFeature(
            fragmentManager,
            store,
            ContextMenuCandidate.defaultCandidates(testContext, mock(), mock(), mock()),
            engineView,
            mock())

        feature.start()
        feature.stop()

        store.dispatch(ContentAction.UpdateHitResultAction(
            "test-tab",
            HitResult.UNKNOWN("https://www.mozilla.org")
        )).joinBlocking()

        testDispatcher.advanceUntilIdle()

        verify(fragmentManager, never()).beginTransaction()
        verify(view, never()).performHapticFeedback(HapticFeedbackConstants.LONG_PRESS)
    }

    @Test
    fun `Feature will re-attach to already existing fragment`() {
        val fragment: ContextMenuFragment = mock()
        doReturn("test-tab").`when`(fragment).sessionId

        val fragmentManager: FragmentManager = mock()
        doReturn(fragment).`when`(fragmentManager).findFragmentByTag(any())

        val (engineView, view) = mockEngineView()

        store.dispatch(ContentAction.UpdateHitResultAction(
            "test-tab",
            HitResult.UNKNOWN("https://www.mozilla.org")
        )).joinBlocking()

        val feature = ContextMenuFeature(
            fragmentManager,
            store,
            ContextMenuCandidate.defaultCandidates(testContext, mock(), mock(), mock()),
            engineView,
            mock())

        feature.start()

        testDispatcher.advanceUntilIdle()

        verify(fragment).feature = feature
        verify(view, never()).performHapticFeedback(HapticFeedbackConstants.LONG_PRESS)
    }

    @Test
    fun `Already existing fragment will be removed if session has no HitResult set anymore`() {
        val fragment: ContextMenuFragment = mock()
        doReturn("test-tab").`when`(fragment).sessionId

        val transaction: FragmentTransaction = mock()

        val fragmentManager: FragmentManager = mock()
        doReturn(fragment).`when`(fragmentManager).findFragmentByTag(any())
        doReturn(transaction).`when`(fragmentManager).beginTransaction()
        doReturn(transaction).`when`(transaction).remove(fragment)

        val (engineView, view) = mockEngineView()

        val feature = ContextMenuFeature(
            fragmentManager,
            store,
            ContextMenuCandidate.defaultCandidates(testContext, mock(), mock(), mock()),
            engineView,
            mock())

        feature.start()

        testDispatcher.advanceUntilIdle()

        verify(fragmentManager).beginTransaction()
        verify(transaction).remove(fragment)

        verify(view, never()).performHapticFeedback(HapticFeedbackConstants.LONG_PRESS)
    }

    fun `Already existing fragment will be removed if session does not exist anymore`() {
        val fragment: ContextMenuFragment = mock()
        doReturn("test-tab").`when`(fragment).sessionId

        val transaction: FragmentTransaction = mock()

        val fragmentManager: FragmentManager = mock()
        doReturn(fragment).`when`(fragmentManager).findFragmentByTag(any())
        doReturn(transaction).`when`(fragmentManager).beginTransaction()
        doReturn(transaction).`when`(transaction).remove(fragment)

        val (engineView, view) = mockEngineView()

        val feature = ContextMenuFeature(
            fragmentManager,
            store,
            ContextMenuCandidate.defaultCandidates(testContext, mock(), mock(), mock()),
            engineView,
            mock())

        store.dispatch(TabListAction.RemoveTabAction("test-tab"))
            .joinBlocking()

        feature.start()

        testDispatcher.advanceUntilIdle()

        verify(fragmentManager).beginTransaction()
        verify(transaction).remove(fragment)

        verify(view, never()).performHapticFeedback(HapticFeedbackConstants.LONG_PRESS)
    }

    @Test
    fun `No dialog will be shown if no item wants to be shown`() {
        val fragmentManager = mockFragmentManager()

        val candidate = ContextMenuCandidate(
            id = "test-id",
            label = "Test Item",
            showFor = { _, _ -> false },
            action = { _, _ -> Unit }
        )

        val (engineView, view) = mockEngineView()

        val feature = ContextMenuFeature(
            fragmentManager,
            store,
            listOf(candidate),
            engineView,
            ContextMenuUseCases(mock(), mock())
        )

        feature.showContextMenu(
            createTab("https://www.mozilla.org"),
            HitResult.UNKNOWN("https://www.mozilla.org"))

        verify(fragmentManager, never()).beginTransaction()
        verify(view, never()).performHapticFeedback(HapticFeedbackConstants.LONG_PRESS)
    }

    @Test
    fun `Cancelling context menu item will consume HitResult`() {
        store = BrowserStore()

        val sessionManager = SessionManager(engine = mock(), store = store)
        Session("https://www.mozilla.org", id = "test-tab").also {
            sessionManager.add(it)
            it.hitResult = Consumable.from(HitResult.UNKNOWN("https://www.mozilla.org"))
        }

        val (engineView, _) = mockEngineView()

        val feature = ContextMenuFeature(
            mockFragmentManager(),
            store,
            ContextMenuCandidate.defaultCandidates(testContext, mock(), mock(), mock()),
            engineView,
            ContextMenuUseCases(sessionManager, store)
        )

        assertNotNull(store.state.findTab("test-tab")!!.content.hitResult)

        feature.onMenuCancelled("test-tab")

        testDispatcher.advanceUntilIdle()

        assertNull(store.state.findTab("test-tab")!!.content.hitResult)
    }

    @Test
    fun `Selecting context menu item will invoke action of candidate and consume HitResult`() {
        store = BrowserStore()

        val sessionManager = SessionManager(engine = mock(), store = store)
        Session("https://www.mozilla.org", id = "test-tab").also {
            sessionManager.add(it)
            it.hitResult = Consumable.from(HitResult.UNKNOWN("https://www.mozilla.org"))
        }

        val (engineView, view) = mockEngineView()
        var actionInvoked = false

        val candidate = ContextMenuCandidate(
            id = "test-id",
            label = "Test Item",
            showFor = { _, _ -> true },
            action = { _, _ -> actionInvoked = true })

        val feature = ContextMenuFeature(
            mockFragmentManager(),
            store,
            listOf(candidate),
            engineView,
            ContextMenuUseCases(sessionManager, store)
        )

        testDispatcher.advanceUntilIdle()

        assertNotNull(store.state.findTab("test-tab")!!.content.hitResult)
        assertFalse(actionInvoked)

        feature.onMenuItemSelected("test-tab", "test-id")

        testDispatcher.advanceUntilIdle()

        assertNull(store.state.findTab("test-tab")!!.content.hitResult)
        assertTrue(actionInvoked)
        verify(view, never()).performHapticFeedback(HapticFeedbackConstants.LONG_PRESS)
    }

    @Test
    fun `Selecting context menu item will emit a click fact`() {
        store = BrowserStore()

        val sessionManager = SessionManager(engine = mock(), store = store)
        Session("https://www.mozilla.org", id = "test-tab").also {
            sessionManager.add(it)
            it.hitResult = Consumable.from(HitResult.UNKNOWN("https://www.mozilla.org"))
        }

        val (engineView, _) = mockEngineView()
        val candidate = ContextMenuCandidate(
            id = "test-id",
            label = "Test Item",
            showFor = { _, _ -> true },
            action = { _, _ -> /* noop */ })

        val feature = ContextMenuFeature(
            mockFragmentManager(),
            store,
            listOf(candidate),
            engineView,
            ContextMenuUseCases(sessionManager, store)
        )

        CollectionProcessor.withFactCollection { facts ->
            feature.onMenuItemSelected("test-tab", candidate.id)

            assertEquals(1, facts.size)

            val fact = facts[0]
            assertEquals(Component.FEATURE_CONTEXTMENU, fact.component)
            assertEquals(Action.CLICK, fact.action)
            assertEquals("item", fact.item)
            assertEquals("test-id", fact.metadata?.get("item"))
        }
    }

    private fun mockFragmentManager(): FragmentManager {
        val fragmentManager: FragmentManager = mock()

        val transaction: FragmentTransaction = mock()
        doReturn(transaction).`when`(fragmentManager).beginTransaction()

        return fragmentManager
    }

    private fun mockEngineView(): Pair<EngineView, View> {
        val actualView: View = mock()

        val engineView = mock<EngineView>().also {
            `when`(it.asView()).thenReturn(actualView)
        }

        return Pair(engineView, actualView)
    }
}
