/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs.toolbar

import android.view.ViewGroup
import android.widget.LinearLayout
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.ui.tabcounter.TabCounter
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class TabCounterToolbarButtonTest {

    @Test
    fun `TabCounter has initial count set`() {
        val sessionManager = SessionManager(mock())
        sessionManager.add(Session("about:blank"))
        sessionManager.add(Session("about:blank"))

        val button = TabCounterToolbarButton(sessionManager) {}

        val view = button.createView(LinearLayout(testContext) as ViewGroup)
            as TabCounter

        assertEquals("2", view.getText())
    }

    @Test
    fun `TabCounter registers on SessionManager with parent view lifetime`() {
        val sessionManager = spy(SessionManager(mock()))
        val button = TabCounterToolbarButton(sessionManager) {}

        val parent = LinearLayout(testContext)
        button.createView(parent)

        verify(sessionManager).register(any(), eq(parent))
    }

    @Test
    fun `TabCounter updates when sessions get added`() {
        val sessionManager = SessionManager(mock())

        val button = TabCounterToolbarButton(sessionManager) {}
        val parent = spy(LinearLayout(testContext))
        doReturn(true).`when`(parent).isAttachedToWindow

        val view = button.createView(parent)
            as TabCounter

        assertEquals("0", view.getText())

        sessionManager.add(Session("about:blank"))

        assertEquals("1", view.getText())

        sessionManager.add(Session("about:blank"))
        sessionManager.add(Session("about:blank"))
        sessionManager.add(Session("about:blank"))

        assertEquals("4", view.getText())
    }

    @Test
    fun `TabCounter updates when sessions get removed`() {
        val sessionManager = SessionManager(mock())

        val session1 = Session("about:blank")
        val session2 = Session("about:blank")
        val session3 = Session("about:blank")

        sessionManager.add(session1)
        sessionManager.add(session2)
        sessionManager.add(session3)

        val button = TabCounterToolbarButton(sessionManager) {}
        val parent = spy(LinearLayout(testContext))
        doReturn(true).`when`(parent).isAttachedToWindow

        val view = button.createView(parent)
            as TabCounter

        assertEquals("3", view.getText())

        sessionManager.remove(session1)

        assertEquals("2", view.getText())

        sessionManager.remove(session2)
        sessionManager.remove(session3)

        assertEquals("0", view.getText())
    }

    @Test
    fun `TabCounter updates when all sessions get removed`() {
        val sessionManager = SessionManager(mock())

        val session1 = Session("about:blank")
        val session2 = Session("about:blank")
        val session3 = Session("about:blank")

        sessionManager.add(session1)
        sessionManager.add(session2)
        sessionManager.add(session3)

        val button = TabCounterToolbarButton(sessionManager) {}
        val parent = spy(LinearLayout(testContext))
        doReturn(true).`when`(parent).isAttachedToWindow

        val view = button.createView(parent)
            as TabCounter

        assertEquals("3", view.getText())

        sessionManager.removeSessions()

        assertEquals("0", view.getText())
    }

    @Test
    fun `TabCounter updates when sessions get restored`() {
        val sessionManager = SessionManager(mock())

        val button = TabCounterToolbarButton(sessionManager) {}
        val parent = spy(LinearLayout(testContext))
        doReturn(true).`when`(parent).isAttachedToWindow

        val view = button.createView(parent)
            as TabCounter

        assertEquals("0", view.getText())

        val snapshot = SessionManager.Snapshot(listOf(
            SessionManager.Snapshot.Item(Session("about:blank")),
            SessionManager.Snapshot.Item(Session("about:blank")),
            SessionManager.Snapshot.Item(Session("about:blank"))
        ), selectedSessionIndex = 0)
        sessionManager.restore(snapshot)

        assertEquals("3", view.getText())
    }

    @Test
    fun `Clicking TabCounter invokes showTabs function`() {
        val sessionManager = SessionManager(mock())

        var callbackInvoked = false
        val button = TabCounterToolbarButton(sessionManager) {
            callbackInvoked = true
        }
        val parent = spy(LinearLayout(testContext))
        doReturn(true).`when`(parent).isAttachedToWindow

        val view = button.createView(parent)
            as TabCounter

        view.performClick()

        assertTrue(callbackInvoked)
    }
}
