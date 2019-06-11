/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs.toolbar

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Test
import org.mockito.Mockito.anyString
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

class TabsToolbarFeatureTest {

    @Test
    fun `feature adds "tabs" button to toolbar`() {
        val toolbar: Toolbar = mock()
        val sessionManager: SessionManager = mock()
        TabsToolbarFeature(toolbar, sessionManager) {}

        verify(toolbar).addBrowserAction(any())
    }

    @Test
    fun `feature does not add tabs button when session is a customtab`() {
        val toolbar: Toolbar = mock()
        val sessionManager: SessionManager = mock()
        val session: Session = mock()
        whenever(sessionManager.findSessionById(anyString())).thenReturn(session)
        whenever(session.isCustomTabSession()).thenReturn(true)

        TabsToolbarFeature(toolbar, sessionManager, "123") {}

        verify(toolbar, never()).addBrowserAction(any())
    }

    @Test
    fun `feature adds tab button when session found but not a customtab`() {
        val toolbar: Toolbar = mock()
        val sessionManager: SessionManager = mock()
        val session: Session = mock()
        whenever(sessionManager.findSessionById(anyString())).thenReturn(session)
        whenever(session.isCustomTabSession()).thenReturn(false)

        TabsToolbarFeature(toolbar, sessionManager, "123") {}

        verify(toolbar).addBrowserAction(any())
    }
}
