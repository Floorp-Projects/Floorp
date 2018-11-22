/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.Session.Source
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.EngineSession
import mozilla.components.support.test.any
import org.junit.Assert.assertEquals
import mozilla.components.support.test.mock
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class TabsUseCasesTest {
    @Test
    fun `SelectTabUseCase - session will be selected in session manager`() {
        val sessionManager: SessionManager = mock()
        val useCases = TabsUseCases(sessionManager)

        val session = Session("A")
        useCases.selectTab.invoke(session)

        verify(sessionManager).select(session)
    }

    @Test
    fun `RemoveTabUseCase - session will be removed in session manager`() {
        val sessionManager: SessionManager = mock()
        val useCases = TabsUseCases(sessionManager)

        val session = Session("A")
        useCases.removeTab.invoke(session)

        verify(sessionManager).remove(session)
    }

    @Test
    fun `AddNewTabUseCase - session will be added to session manager`() {
        val sessionManager = spy(SessionManager(mock()))
        val engineSession: EngineSession = mock()
        doReturn(engineSession).`when`(sessionManager).getOrCreateEngineSession(any())

        val useCases = TabsUseCases(sessionManager)

        assertEquals(0, sessionManager.size)

        useCases.addTab.invoke("https://www.mozilla.org")

        assertEquals(1, sessionManager.size)
        assertEquals("https://www.mozilla.org", sessionManager.selectedSessionOrThrow.url)
        assertEquals(Source.NEW_TAB, sessionManager.selectedSessionOrThrow.source)
        assertFalse(sessionManager.selectedSessionOrThrow.private)

        verify(engineSession).loadUrl("https://www.mozilla.org")
    }

    @Test
    fun `AddNewPrivateTabUseCase - private session will be added to session manager`() {
        val sessionManager = spy(SessionManager(mock()))
        val engineSession: EngineSession = mock()
        doReturn(engineSession).`when`(sessionManager).getOrCreateEngineSession(any())

        val useCases = TabsUseCases(sessionManager)

        assertEquals(0, sessionManager.size)

        useCases.addPrivateTab.invoke("https://www.mozilla.org")

        assertEquals(1, sessionManager.size)
        assertEquals("https://www.mozilla.org", sessionManager.selectedSessionOrThrow.url)
        assertEquals(Source.NEW_TAB, sessionManager.selectedSessionOrThrow.source)
        assertTrue(sessionManager.selectedSessionOrThrow.private)

        verify(engineSession).loadUrl("https://www.mozilla.org")
    }

    @Test
    fun `AddNewTabUseCase will not load URL if flag is set to false`() {
        val sessionManager = spy(SessionManager(mock()))
        val engineSession: EngineSession = mock()
        doReturn(engineSession).`when`(sessionManager).getOrCreateEngineSession(any())

        val useCases = TabsUseCases(sessionManager)

        useCases.addTab.invoke("https://www.mozilla.org", startLoading = false)

        verify(engineSession, never()).loadUrl("https://www.mozilla.org")
    }

    @Test
    fun `AddNewPrivateTabUseCase will not load URL if flag is set to false`() {
        val sessionManager = spy(SessionManager(mock()))
        val engineSession: EngineSession = mock()
        doReturn(engineSession).`when`(sessionManager).getOrCreateEngineSession(any())

        val useCases = TabsUseCases(sessionManager)

        useCases.addPrivateTab.invoke("https://www.mozilla.org", startLoading = false)

        verify(engineSession, never()).loadUrl("https://www.mozilla.org")
    }
}
