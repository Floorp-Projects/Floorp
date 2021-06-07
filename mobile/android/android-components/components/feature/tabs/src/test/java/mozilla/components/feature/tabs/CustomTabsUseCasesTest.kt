/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.support.test.mock
import org.junit.Assert.assertNull
import org.junit.Test
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

class CustomTabsUseCasesTest {

    @Test
    fun `MigrateCustomTabUseCase - turns custom tab into regular tab and selects it`() {
        val sessionManager: SessionManager = mock()
        val useCases = CustomTabsUseCases(sessionManager, mock())

        val session1 = Session("CustomTabSession1")
        session1.customTabConfig = mock()
        doReturn(session1).`when`(sessionManager).findSessionById(session1.id)

        useCases.migrate(session1.id, select = false)
        assertNull(session1.customTabConfig)
        verify(sessionManager, never()).select(session1)

        val session2 = Session("CustomTabSession2")
        session2.customTabConfig = mock()
        doReturn(session2).`when`(sessionManager).findSessionById(session2.id)

        useCases.migrate(session2.id)
        assertNull(session2.customTabConfig)
        verify(sessionManager).select(session2)
    }
}
