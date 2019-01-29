/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.Session.FindResult
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.feature.findinpage.FindInPageFeature
import mozilla.components.feature.findinpage.FindInPageView
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.mock
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class FindInPageFeatureTest {

    private lateinit var mockSessionManager: SessionManager
    private lateinit var mockFindInPageView: FindInPageView
    private lateinit var feature: FindInPageFeature

    @Before
    fun setup() {
        val engine = Mockito.mock(Engine::class.java)
        mockSessionManager = Mockito.spy(SessionManager(engine))
        mockFindInPageView = Mockito.mock(FindInPageView::class.java)
        feature = FindInPageFeature(mockSessionManager, mockFindInPageView)
    }

    @Test
    fun `When the feature is started onFindResult events must be forwarded to the findInPageView`() {
        feature.start()
        val session = selectNewSession()

        session.findResults = listOf(FindResult(0, 1, false))

        verify(mockFindInPageView).onFindResultReceived(session.findResults.first())
    }

    @Test
    fun `After stopping the feature no new onFindResult events will be forwarded to the findInPageView`() {
        feature.start()
        feature.stop()

        val session = selectNewSession()

        session.findResults = listOf(FindResult(0, 1, false))

        verify(mockFindInPageView, times(0)).onFindResultReceived(session.findResults.first())
    }

    @Test
    fun `When starting the feature with a selected session the findInPageView engineSession must be assigned`() {
        val mockEngineSession = mock(EngineSession::class.java)
        val selectedSession = selectNewSession()

        doReturn(mockEngineSession).`when`(mockSessionManager).getEngineSession(selectedSession)

        feature.start()

        verify(mockFindInPageView).engineSession = mockEngineSession
    }

    @Test
    fun `When a new session is selected the findInPageView engineSession must be assigned`() {
        val mockEngineSession = mock(EngineSession::class.java)
        val session = Session("")

        feature.start()

        doReturn(mockEngineSession).`when`(mockSessionManager).getEngineSession(session)

        selectNewSession(session)
        verify(mockFindInPageView, times(2)).engineSession = mockEngineSession
    }

    @Test
    fun `Calling onBackPressed will notify the findInPageView when the widget is active`() {

        feature.start()

        val session = selectNewSession()

        session.findResults = listOf(FindResult(0, 1, false))

        doReturn(true).`when`(mockFindInPageView).isActive()

        val backEventWasHandled = feature.onBackPressed()

        assertTrue(backEventWasHandled)

        verify(mockFindInPageView).onCloseButtonClicked()
    }

    @Test
    fun `Calling onBackPressed will not notify the findInPageView when the widget inactive`() {

        feature.start()

        val session = selectNewSession()

        session.findResults = listOf(FindResult(0, 1, false))

        val backEventWasHandled = feature.onBackPressed()

        assertFalse(backEventWasHandled)

        verify(mockFindInPageView, times(0)).onCloseButtonClicked()
    }

    private fun selectNewSession(session: Session = Session("")): Session {
        mockSessionManager.add(session)
        mockSessionManager.select(session)
        return session
    }
}
