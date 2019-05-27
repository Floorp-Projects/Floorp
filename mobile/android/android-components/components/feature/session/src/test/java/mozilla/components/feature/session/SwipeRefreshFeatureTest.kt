/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import android.content.Context
import android.graphics.Bitmap
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineView
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class SwipeRefreshFeatureTest {
    private lateinit var refreshFeature: SwipeRefreshFeature
    private lateinit var mockSessionManager: SessionManager
    private val mockLayout = mock(SwipeRefreshLayout::class.java)
    private val mockSession = mock(Session::class.java)
    private val useCase = mock(SessionUseCases.ReloadUrlUseCase::class.java)

    @Before
    fun setup() {
        mockSessionManager = mock(SessionManager::class.java)
        refreshFeature = SwipeRefreshFeature(mockSessionManager, useCase, mockLayout)

        `when`(mockSessionManager.selectedSession).thenReturn(mockSession)
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

        verify(useCase).invoke(mockSession)
    }

    @Test
    fun `onLoadingStateChanged should update the SwipeRefreshLayout`() {
        refreshFeature.onLoadingStateChanged(mockSession, false)
        verify(mockLayout).isRefreshing = false

        refreshFeature.onLoadingStateChanged(mockSession, true)
        verify(mockLayout).isRefreshing = false
    }

    @Test
    fun `start with a sessionId`() {
        refreshFeature = spy(SwipeRefreshFeature(mockSessionManager, useCase, mockLayout, "abc"))
        `when`(mockSessionManager.findSessionById(anyString())).thenReturn(mockSession)

        refreshFeature.start()

        verify(refreshFeature).observeIdOrSelected(anyString())
        verify(mockSessionManager).findSessionById(anyString())
        verify(refreshFeature).observeFixed(mockSession)
    }

    private open class DummyEngineView(context: Context) : FrameLayout(context), EngineView {
        override fun canScrollVerticallyUp() = scrollY > 0
        override fun setVerticalClipping(clippingHeight: Int) {}
        override fun captureThumbnail(onFinish: (Bitmap?) -> Unit) = Unit
        override fun render(session: EngineSession) {}
    }
}
