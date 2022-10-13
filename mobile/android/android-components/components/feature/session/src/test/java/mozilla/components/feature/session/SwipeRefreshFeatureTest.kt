/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import android.content.Context
import android.graphics.Bitmap
import android.widget.FrameLayout
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.selector.findCustomTabOrSelectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.engine.InputResultDetail
import mozilla.components.concept.engine.selection.SelectionActionDelegate
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.reset
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class SwipeRefreshFeatureTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private lateinit var store: BrowserStore
    private lateinit var refreshFeature: SwipeRefreshFeature
    private val mockLayout = mock<SwipeRefreshLayout>()
    private val useCase = mock<SessionUseCases.ReloadUrlUseCase>()

    @Before
    fun setup() {
        store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "A"),
                    createTab("https://www.firefox.com", id = "B"),
                ),
                selectedTabId = "B",
            ),
        )

        refreshFeature = SwipeRefreshFeature(store, useCase, mockLayout)
    }

    @Test
    fun `sets the onRefreshListener and onChildScrollUpCallback`() {
        verify(mockLayout).setOnRefreshListener(refreshFeature)
        verify(mockLayout).setOnChildScrollUpCallback(refreshFeature)
    }

    @Test
    fun `gesture should only work if the content can be overscrolled`() {
        val engineView: DummyEngineView = mock()
        val inputResultDetail: InputResultDetail = mock()
        doReturn(inputResultDetail).`when`(engineView).getInputResultDetail()

        doReturn(true).`when`(inputResultDetail).canOverscrollTop()
        assertFalse(refreshFeature.canChildScrollUp(mockLayout, engineView))

        doReturn(false).`when`(inputResultDetail).canOverscrollTop()
        assertTrue(refreshFeature.canChildScrollUp(mockLayout, engineView))
    }

    @Test
    fun `onRefresh should refresh the active session`() {
        refreshFeature.start()
        refreshFeature.onRefresh()

        verify(useCase).invoke("B")
    }

    @Test
    fun `feature MUST reset refreshCanceled after is used`() {
        refreshFeature.start()

        val selectedTab = store.state.findCustomTabOrSelectedTab()!!

        store.dispatch(ContentAction.UpdateRefreshCanceledStateAction(selectedTab.id, true)).joinBlocking()
        store.waitUntilIdle()

        assertFalse(selectedTab.content.refreshCanceled)
    }

    @Test
    fun `feature clears the swipeRefreshLayout#isRefreshing when tab fishes loading or a refreshCanceled`() {
        refreshFeature.start()
        store.waitUntilIdle()

        val selectedTab = store.state.findCustomTabOrSelectedTab()!!

        // Ignoring the first event from the initial state.
        reset(mockLayout)

        store.dispatch(ContentAction.UpdateRefreshCanceledStateAction(selectedTab.id, true)).joinBlocking()
        store.waitUntilIdle()

        verify(mockLayout, times(2)).isRefreshing = false

        // To trigger to an event we have to change loading from its previous value (false to true).
        // As if we dispatch with loading = false, none event will be trigger.
        store.dispatch(ContentAction.UpdateLoadingStateAction(selectedTab.id, true)).joinBlocking()
        store.dispatch(ContentAction.UpdateLoadingStateAction(selectedTab.id, false)).joinBlocking()

        verify(mockLayout, times(3)).isRefreshing = false
    }

    private open class DummyEngineView(context: Context) : FrameLayout(context), EngineView {
        override fun setVerticalClipping(clippingHeight: Int) {}
        override fun setDynamicToolbarMaxHeight(height: Int) {}
        override fun captureThumbnail(onFinish: (Bitmap?) -> Unit) = Unit
        override fun clearSelection() {}
        override fun render(session: EngineSession) {}
        override fun release() {}
        override var selectionActionDelegate: SelectionActionDelegate? = null
    }
}
