/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.feature.toolbar

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.setMain
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.content.blocking.Tracker
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.feature.toolbar.internal.URLRenderer
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.anyString
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

class ToolbarPresenterTest {

    @Before
    @ExperimentalCoroutinesApi
    fun setUp() {
        // Execute main thread coroutines on same thread as caller.
        Dispatchers.setMain(Dispatchers.Unconfined)
    }

    @After
    @ExperimentalCoroutinesApi
    fun tearDown() {
        Dispatchers.resetMain()
    }

    @Test
    fun `start with no sessionId`() {
        val toolbar: Toolbar = mock()
        val sessionManager: SessionManager = mock()
        val toolbarPresenter = spy(ToolbarPresenter(toolbar, sessionManager))

        toolbarPresenter.start()

        verify(toolbarPresenter).observeIdOrSelected(null)
        verify(toolbarPresenter).observeSelected()
        verify(toolbarPresenter).initializeView()
    }

    @Test
    fun `start with sessionId`() {
        val toolbar: Toolbar = mock()
        val sessionManager: SessionManager = mock()
        val session = Session("https://mozilla.org")
        val toolbarPresenter = spy(ToolbarPresenter(toolbar, sessionManager, "123"))

        whenever(sessionManager.findSessionById("123")).thenReturn(session)

        toolbarPresenter.start()

        verify(toolbarPresenter).observeIdOrSelected("123")
        verify(toolbarPresenter).observeFixed(session)
        verify(toolbarPresenter).initializeView()
    }

    @Test
    fun `start calls initializeView and setup methods`() {
        val toolbar: Toolbar = mock()
        val sessionManager: SessionManager = mock()
        val session = Session("https://mozilla.org")
        val toolbarPresenter = spy(ToolbarPresenter(toolbar, sessionManager, "123"))
        toolbarPresenter.observeFixed(session)

        toolbarPresenter.start()

        verify(toolbarPresenter).initializeView()
        verify(toolbar).siteSecure = Toolbar.SiteSecurity.INSECURE
        verify(toolbar).url = anyString()
    }

    @Test
    fun `onSecurityChanged updates toolbar`() {
        val toolbar: Toolbar = mock()
        val sessionManager: SessionManager = mock()
        val session = Session("https://mozilla.org")
        val toolbarPresenter = spy(ToolbarPresenter(toolbar, sessionManager, "123"))

        toolbarPresenter.onSecurityChanged(session, Session.SecurityInfo(true))

        verify(toolbar).siteSecure = Toolbar.SiteSecurity.SECURE

        toolbarPresenter.onSecurityChanged(session, Session.SecurityInfo(false))

        verify(toolbar).siteSecure = Toolbar.SiteSecurity.INSECURE
    }

    @Test
    fun `onTrackerBlockingEnabledChanged,onTrackerBlocked and onUrlChanged updates toolbar`() {
        val toolbar: Toolbar = mock()
        val sessionManager: SessionManager = mock()
        val session = Session("https://mozilla.org")
        val toolbarPresenter = spy(ToolbarPresenter(toolbar, sessionManager, "123"))

        session.trackerBlockingEnabled = true
        toolbarPresenter.onUrlChanged(session, "https://mozilla.org")

        verify(toolbar).siteTrackingProtection = Toolbar.SiteTrackingProtection.ON_NO_TRACKERS_BLOCKED

        session.trackerBlockingEnabled = false
        toolbarPresenter.onTrackerBlockingEnabledChanged(session, false)

        verify(toolbar).siteTrackingProtection = Toolbar.SiteTrackingProtection.OFF_GLOBALLY

        session.trackerBlockingEnabled = true
        session.trackersBlocked = listOf(Tracker("tracker_url"))
        toolbarPresenter.onTrackerBlocked(session, Tracker("tracker_url"), session.trackersBlocked)

        verify(toolbar).siteTrackingProtection = Toolbar.SiteTrackingProtection.ON_TRACKERS_BLOCKED
    }

    @Test
    fun `Toolbar displays empty state on start`() {
        val toolbar: Toolbar = mock()
        val sessionManager: SessionManager = mock()

        val presenter = ToolbarPresenter(toolbar, sessionManager)
        presenter.start()

        verify(toolbar).url = ""
        verify(toolbar).displayProgress(0)
    }

    @Test
    fun `Toolbar gets cleared when all sessions get removed`() {
        val toolbar: Toolbar = mock()

        val sessionManager: SessionManager = mock()

        val session = Session("https://www.mozilla.org")
        doReturn(session).`when`(sessionManager).selectedSession

        val presenter = ToolbarPresenter(toolbar, sessionManager)
        presenter.start()

        verify(toolbar).url = "https://www.mozilla.org"

        doReturn(null).`when`(sessionManager).selectedSession
        presenter.onAllSessionsRemoved()

        verify(toolbar).url = ""
    }

    @Test
    fun `Toolbar gets cleared when selected session gets removed`() {
        val toolbar: Toolbar = mock()

        val sessionManager: SessionManager = mock()

        val session = Session("https://www.mozilla.org")
        doReturn(session).`when`(sessionManager).selectedSession

        val presenter = ToolbarPresenter(toolbar, sessionManager)
        presenter.start()

        verify(toolbar).url = "https://www.mozilla.org"

        doReturn(null).`when`(sessionManager).selectedSession
        presenter.onSessionRemoved(session)

        verify(toolbar).url = ""
    }

    @Test
    fun `Search terms get forwarded to toolbar`() {
        val toolbar: Toolbar = mock()
        val sessionManager: SessionManager = mock()

        val presenter = ToolbarPresenter(toolbar, sessionManager)
        presenter.onSearch(Session("https://www.mozilla.org"), "hello world")

        verify(toolbar).setSearchTerms("hello world")
    }

    @Test
    fun `Toolbar gets not cleared if a background session gets removed`() {
        val toolbar: Toolbar = mock()

        val sessionManager: SessionManager = mock()

        val session = Session("https://www.mozilla.org")
        doReturn(session).`when`(sessionManager).selectedSession

        val presenter = ToolbarPresenter(toolbar, sessionManager)
        presenter.start()

        verify(toolbar).url = "https://www.mozilla.org"

        presenter.onSessionRemoved(Session("https://www.firefox.com"))

        verify(toolbar, never()).url = ""
        verify(toolbar, never()).url = "https://www.firefox.com"
    }

    @Test
    fun `Presenter initializes view if new session gets selected`() {
        val presenter = spy(ToolbarPresenter(mock(), mock()))

        presenter.onSessionSelected(mock())

        verify(presenter).initializeView()
    }

    @Test
    fun `Presenter forwards progress to toolbar`() {
        val toolbar: Toolbar = mock()
        val presenter = ToolbarPresenter(toolbar, mock())

        presenter.onProgress(mock(), 79)

        verify(toolbar).displayProgress(79)
    }

    @Test
    fun `Presenter starts renderer`() {
        val presenter = ToolbarPresenter(mock(), mock())

        val renderer: URLRenderer = mock()
        presenter.renderer = renderer

        presenter.start()
        verify(renderer).start()
        verify(renderer).post(anyString())
    }

    @Test
    fun `Presenter stops renderer`() {
        val presenter = ToolbarPresenter(mock(), mock())

        val renderer: URLRenderer = mock()
        presenter.renderer = renderer

        presenter.stop()
        verify(renderer).stop()
    }

    @Test
    fun `Presenter posts URL to renderer`() {
        val presenter = ToolbarPresenter(mock(), mock())

        val renderer: URLRenderer = mock()
        presenter.renderer = renderer

        presenter.onUrlChanged(mock(), "https://www.mozilla.org")

        verify(renderer).post("https://www.mozilla.org")
    }
}
