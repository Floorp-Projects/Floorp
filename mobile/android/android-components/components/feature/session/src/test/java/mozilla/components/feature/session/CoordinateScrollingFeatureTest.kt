/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import android.os.Looper.getMainLooper
import android.view.View
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.google.android.material.appbar.AppBarLayout
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineView
import mozilla.components.feature.session.CoordinateScrollingFeature.Companion.DEFAULT_SCROLL_FLAGS
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.any
import org.mockito.Mockito.verify
import org.robolectric.Shadows.shadowOf

@RunWith(AndroidJUnit4::class)
class CoordinateScrollingFeatureTest {

    private lateinit var scrollFeature: CoordinateScrollingFeature
    private lateinit var mockEngineView: EngineView
    private lateinit var mockView: View
    private lateinit var store: BrowserStore

    @Before
    fun setup() {
        store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "mozilla"),
                ),
                selectedTabId = "mozilla",
            ),
        )

        mockEngineView = mock()
        mockView = mock()
        scrollFeature = CoordinateScrollingFeature(store, mockEngineView, mockView)

        whenever(mockView.layoutParams).thenReturn(mock<AppBarLayout.LayoutParams>())
    }

    @Test
    fun `when session loading StateChanged and engine canScrollVertically is false must remove scrollFlags`() {
        scrollFeature.start()
        shadowOf(getMainLooper()).idle()

        store.dispatch(ContentAction.UpdateLoadingStateAction("mozilla", true)).joinBlocking()

        verify((mockView.layoutParams as AppBarLayout.LayoutParams)).scrollFlags = 0
        verify(mockView).layoutParams = any()
    }

    @Test
    fun `when session loading StateChanged and engine canScrollVertically is true must add DEFAULT_SCROLL_FLAGS `() {
        whenever(mockEngineView.canScrollVerticallyDown()).thenReturn(true)

        scrollFeature.start()
        shadowOf(getMainLooper()).idle()

        store.dispatch(ContentAction.UpdateLoadingStateAction("mozilla", true)).joinBlocking()

        verify((mockView.layoutParams as AppBarLayout.LayoutParams)).scrollFlags = DEFAULT_SCROLL_FLAGS
        verify(mockView).layoutParams = any()
    }

    @Test
    fun `when session loading StateChanged and engine canScrollVertically is true must add custom scrollFlags`() {
        whenever(mockEngineView.canScrollVerticallyDown()).thenReturn(true)
        scrollFeature = CoordinateScrollingFeature(store, mockEngineView, mockView, 12)
        scrollFeature.start()
        shadowOf(getMainLooper()).idle()

        store.dispatch(ContentAction.UpdateLoadingStateAction("mozilla", true)).joinBlocking()

        verify((mockView.layoutParams as AppBarLayout.LayoutParams)).scrollFlags = 12
        verify(mockView).layoutParams = any()
    }
}
