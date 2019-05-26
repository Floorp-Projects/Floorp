/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import android.view.View
import com.google.android.material.appbar.AppBarLayout
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineView
import mozilla.components.feature.session.CoordinateScrollingFeature.Companion.DEFAULT_SCROLL_FLAGS
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.any
import org.mockito.Mockito.mock
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class CoordinateScrollingFeatureTest {

    private lateinit var scrollFeature: CoordinateScrollingFeature
    private lateinit var mockSessionManager: SessionManager
    private lateinit var mockEngineView: EngineView
    private lateinit var mockView: View

    @Before
    fun setup() {
        val engine = mock(Engine::class.java)
        mockSessionManager = spy(SessionManager(engine))
        mockEngineView = mock(EngineView::class.java)
        mockView = mock(View::class.java)
        scrollFeature = CoordinateScrollingFeature(mockSessionManager, mockEngineView, mockView)

        `when`(mockView.layoutParams).thenReturn(mock(AppBarLayout.LayoutParams::class.java))
    }

    @Test
    fun `when session loading StateChanged and engine canScrollVertically is false must remove scrollFlags`() {

        val session = getSelectedSession()
        scrollFeature.start()

        session.notifyObservers {
            onLoadingStateChanged(session, true)
        }

        verify((mockView.layoutParams as AppBarLayout.LayoutParams)).scrollFlags = 0
        verify(mockView).layoutParams = any()
    }

    @Test
    fun `when session loading StateChanged and engine canScrollVertically is true must add DEFAULT_SCROLL_FLAGS `() {

        val session = getSelectedSession()
        `when`(mockEngineView.canScrollVerticallyDown()).thenReturn(true)

        scrollFeature.start()

        session.notifyObservers {
            onLoadingStateChanged(session, true)
        }

        verify((mockView.layoutParams as AppBarLayout.LayoutParams)).scrollFlags = DEFAULT_SCROLL_FLAGS
        verify(mockView).layoutParams = any()
    }

    @Test
    fun `when session loading StateChanged and engine canScrollVertically is true must add custom scrollFlags`() {

        val session = getSelectedSession()
        `when`(mockEngineView.canScrollVerticallyDown()).thenReturn(true)
        scrollFeature = CoordinateScrollingFeature(mockSessionManager, mockEngineView, mockView, 12)
        scrollFeature.start()

        session.notifyObservers {
            onLoadingStateChanged(session, true)
        }

        verify((mockView.layoutParams as AppBarLayout.LayoutParams)).scrollFlags = 12
        verify(mockView).layoutParams = any()
    }

    private fun getSelectedSession(): Session {
        val session = Session("")
        mockSessionManager.add(session)
        mockSessionManager.select(session)
        return session
    }
}