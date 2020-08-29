/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.recentlyclosed

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.flow
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.RecentlyClosedAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ClosedTabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.junit.After
import org.junit.Assert
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito
import org.mockito.Mockito.verify

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class RecentlyClosedMiddlewareTest {
    lateinit var store: BrowserStore
    lateinit var context: MiddlewareContext<BrowserState, BrowserAction>
    lateinit var engine: Engine

    private val dispatcher = TestCoroutineDispatcher()
    private val scope = CoroutineScope(dispatcher)

    @After
    fun tearDown() {
        dispatcher.cleanupTestCoroutines()
    }

    @Before
    fun setup() {
        store = mock()
        context = mock()
        engine = mock()

        whenever(context.store).thenReturn(store)
    }

    // Test tab
    private val closedTab = ClosedTabSessionState(
        id = "tab-id",
        title = "Mozilla",
        url = "https://mozilla.org",
        createdAt = 1234
    )

    @Test
    fun `closed tab storage stores the provided tab on add tab action`() =
        runBlockingTest {
            val storage: RecentlyClosedTabsStorage = mock()
            val middleware = Mockito.spy(RecentlyClosedMiddleware(testContext, 5, engine, scope))
            val store = BrowserStore(
                initialState = BrowserState(),
                middleware = listOf(middleware)
            )

            whenever(middleware.recentlyClosedTabsStorage).thenReturn(storage)

            store.dispatch(RecentlyClosedAction.AddClosedTabsAction(listOf(closedTab)))
                .joinBlocking()

            dispatcher.advanceUntilIdle()

            verify(middleware.recentlyClosedTabsStorage).addTabsToCollectionWithMax(
                listOf(closedTab), 5
            )
        }

    @Test
    fun `fetch the tabs from the recently closed storage and load into browser state on initialize tab state action`() =
        runBlockingTest {
            val storage: RecentlyClosedTabsStorage = mock()
            val middleware = Mockito.spy(RecentlyClosedMiddleware(testContext, 5, engine, scope))
            val store = Mockito.spy(
                BrowserStore(
                    initialState = BrowserState(),
                    middleware = listOf(middleware)
                )
            )

            whenever(middleware.recentlyClosedTabsStorage).thenReturn(storage)
            whenever(storage.getTabs()).thenReturn(
                flow {
                    emit(listOf(closedTab))
                }
            )

            store.dispatch(RecentlyClosedAction.InitializeRecentlyClosedState).joinBlocking()

            dispatcher.advanceUntilIdle()

            verify(storage).getTabs()
            Assert.assertEquals(closedTab, store.state.closedTabs[0])
        }

    @Test
    fun `recently closed storage removes the provided tab on remove tab action`() =
        runBlockingTest {
            val storage: RecentlyClosedTabsStorage = mock()
            val middleware = Mockito.spy(RecentlyClosedMiddleware(testContext, 5, engine, scope))
            val store = BrowserStore(
                initialState = BrowserState(
                    closedTabs = listOf(
                        closedTab
                    )
                ),
                middleware = listOf(middleware)
            )

            whenever(middleware.recentlyClosedTabsStorage).thenReturn(storage)

            store.dispatch(RecentlyClosedAction.RemoveClosedTabAction(closedTab))
                .joinBlocking()

            verify(storage).removeTab(closedTab)
        }

    @Test
    fun `recently closed storage removes all tabs on remove all tabs action`() =
        runBlockingTest {
            val storage: RecentlyClosedTabsStorage = mock()
            val middleware = Mockito.spy(RecentlyClosedMiddleware(testContext, 5, engine, scope))
            val store = BrowserStore(
                initialState = BrowserState(
                    closedTabs = listOf(
                        closedTab
                    )
                ),
                middleware = listOf(middleware)
            )

            whenever(middleware.recentlyClosedTabsStorage).thenReturn(storage)

            store.dispatch(RecentlyClosedAction.RemoveAllClosedTabAction).joinBlocking()

            verify(storage).removeAllTabs()
        }
}