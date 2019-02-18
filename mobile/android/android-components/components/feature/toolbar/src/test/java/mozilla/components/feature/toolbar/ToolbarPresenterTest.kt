/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.feature.toolbar

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.support.test.mock
import org.junit.Test
import org.mockito.Mockito.`when`
import org.mockito.Mockito.anyString
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy

import org.mockito.Mockito.verify

class ToolbarPresenterTest {

    @Test
    fun `start with no sessionId`() {
        val toolbar: Toolbar = mock()
        val sessionManager: SessionManager = mock()
        val toolbarPresenter = spy(ToolbarPresenter(toolbar, sessionManager))

        toolbarPresenter.start()

        verify(toolbarPresenter).observeSelected()
        verify(toolbarPresenter).initializeView()
    }

    @Test
    fun `start with sessionId`() {
        val toolbar: Toolbar = mock()
        val sessionManager: SessionManager = mock()
        val session = Session("https://mozilla.org")
        val toolbarPresenter = spy(ToolbarPresenter(toolbar, sessionManager, "123"))

        `when`(sessionManager.findSessionById("123")).thenReturn(session)

        toolbarPresenter.start()

        verify(toolbarPresenter).observeFixed(session)
        verify(toolbarPresenter).initializeView()
    }

    @Test
    fun `initializeView calls setup methods`() {
        val toolbar: Toolbar = mock()
        val sessionManager: SessionManager = mock()
        val session = Session("https://mozilla.org")
        val toolbarPresenter = spy(ToolbarPresenter(toolbar, sessionManager, "123"))
        toolbarPresenter.observeFixed(session)

        toolbarPresenter.initializeView()

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
}
