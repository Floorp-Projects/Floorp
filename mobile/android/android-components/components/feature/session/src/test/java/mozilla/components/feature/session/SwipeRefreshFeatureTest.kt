/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import android.content.Context
import android.graphics.Bitmap
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.engine.selection.SelectionActionDelegate
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class SwipeRefreshFeatureTest {

    private lateinit var refreshFeature: SwipeRefreshFeature
    private val mockLayout = mock<SwipeRefreshLayout>()
    private val useCase = mock<SessionUseCases.ReloadUrlUseCase>()

    @Before
    fun setup() {
        val store = BrowserStore(BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "A"),
                createTab("https://www.firefox.com", id = "B")
            ),
            selectedTabId = "B"
        ))

        refreshFeature = SwipeRefreshFeature(store, useCase, mockLayout)
    }

    @Test
    fun `sets the onRefreshListener and onChildScrollUpCallback`() {
        verify(mockLayout).setOnRefreshListener(refreshFeature)
        verify(mockLayout).setOnChildScrollUpCallback(refreshFeature)
    }

    @Test
    fun `gesture should only work if EngineView cannot be scrolled up`() {
        val engineView = DummyEngineView(testContext).apply {
            layoutParams = ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT)
        }

        engineView.scrollY = 0
        assertFalse(refreshFeature.canChildScrollUp(mockLayout, engineView))

        engineView.scrollY = 100
        assertTrue(refreshFeature.canChildScrollUp(mockLayout, engineView))
    }

    @Test
    fun `onRefresh should refresh the active session`() {
        refreshFeature.start()
        refreshFeature.onRefresh()

        verify(useCase).invoke("B")
    }

    @Test
    fun `onLoadingStateChanged should update the SwipeRefreshLayout`() {
        refreshFeature.onLoadingStateChanged(false)
        verify(mockLayout).isRefreshing = false

        refreshFeature.onLoadingStateChanged(true)
        verify(mockLayout).isRefreshing = false
    }

    private open class DummyEngineView(context: Context) : FrameLayout(context), EngineView {
        override fun canScrollVerticallyUp() = scrollY > 0
        override fun setVerticalClipping(clippingHeight: Int) {}
        override fun setDynamicToolbarMaxHeight(height: Int) {}
        override fun captureThumbnail(onFinish: (Bitmap?) -> Unit) = Unit
        override fun clearSelection() {}
        override fun render(session: EngineSession) {}
        override fun release() {}
        override var selectionActionDelegate: SelectionActionDelegate? = null
    }
}
