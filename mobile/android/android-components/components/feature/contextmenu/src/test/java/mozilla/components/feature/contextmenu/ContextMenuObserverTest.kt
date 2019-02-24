/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.feature.contextmenu

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.support.test.mock
import org.junit.Test
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.`when`
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

class ContextMenuObserverTest {
    @Test
    fun `when valid sessionId is provided, observe it's session`() {
        val sessionManager: SessionManager = mock()
        val observer = spy(ContextMenuObserver(sessionManager, mock()))
        val session: Session = mock()

        `when`(sessionManager.findSessionById(anyString())).thenReturn(session)

        observer.start("123")

        verify(observer).observeIdOrSelected("123")
        verify(observer).observeFixed(session)
    }

    @Test
    fun `when sessionId is NOT provided, observe selected session`() {
        val sessionManager: SessionManager = mock()
        val observer = spy(ContextMenuObserver(sessionManager, mock()))

        observer.start(null)

        verify(observer).observeIdOrSelected(null)
        verify(observer).observeSelected()
    }
}