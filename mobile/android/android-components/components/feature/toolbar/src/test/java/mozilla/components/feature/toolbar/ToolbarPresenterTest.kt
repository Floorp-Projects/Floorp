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
}
