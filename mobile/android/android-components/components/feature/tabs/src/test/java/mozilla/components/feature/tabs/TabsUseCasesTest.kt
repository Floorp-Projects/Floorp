/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.Session.Source
import mozilla.components.browser.session.SessionManager
import org.junit.Assert.assertEquals
import mozilla.components.support.test.mock
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class TabsUseCasesTest {
    @Test
    fun `SelectTabUseCase - session will be selected in session manager`() {
        val sessionManager: SessionManager = mock()
        val useCases = TabsUseCases(sessionManager)

        val session = Session("A")
        useCases.selectSession.invoke(session)

        verify(sessionManager).select(session)
    }

    @Test
    fun `RemoveTabUseCase - session will be removed in session manager`() {
        val sessionManager: SessionManager = mock()
        val useCases = TabsUseCases(sessionManager)

        val session = Session("A")
        useCases.removeSession.invoke(session)

        verify(sessionManager).remove(session)
    }

    @Test
    fun `AddNewTabUseCase - session will be added to session manager`() {
        val sessionManager = SessionManager(mock())
        val useCases = TabsUseCases(sessionManager)

        assertEquals(0, sessionManager.size)

        useCases.addSession.invoke("https://www.mozilla.org")

        assertEquals(1, sessionManager.size)
        assertEquals("https://www.mozilla.org", sessionManager.selectedSessionOrThrow.url)
        assertEquals(Source.NEW_TAB, sessionManager.selectedSessionOrThrow.source)
    }
}
