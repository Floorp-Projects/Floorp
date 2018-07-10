/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs.tabstray

import android.arch.lifecycle.LifecycleOwner
import android.view.View
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.tabstray.TabsTray
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class TabsTrayPresenterTest {
    @Test
    fun `start and stop will register and unregister`() {
        val sessionManager: SessionManager = mock()
        val presenter = TabsTrayPresenter(mock(), sessionManager, mock())

        verify(sessionManager, never()).register(presenter)
        verify(sessionManager, never()).unregister(presenter)

        presenter.start()
        verify(sessionManager).register(presenter)
        verify(sessionManager, never()).unregister(presenter)

        presenter.stop()
        verify(sessionManager).unregister(presenter)
    }

    @Test
    fun `initial set of sessions will be passed to tabs tray`() {
        val sessionManager = SessionManager(engine = mock())

        sessionManager.add(Session("https://www.mozilla.org"))
        sessionManager.add(Session("https://getpocket.com"))

        val tabsTray: MockedTabsTray = spy(MockedTabsTray())
        val presenter = TabsTrayPresenter(tabsTray, sessionManager, mock())

        verifyNoMoreInteractions(tabsTray)

        presenter.start()

        assertNotNull(tabsTray.displaySessionsList)
        assertNotNull(tabsTray.displaySelectedIndex)

        assertEquals(0, tabsTray.displaySelectedIndex)
        assertEquals(2, tabsTray.displaySessionsList!!.size)
        assertEquals("https://www.mozilla.org", tabsTray.displaySessionsList!![0].url)
        assertEquals("https://getpocket.com", tabsTray.displaySessionsList!![1].url)

        presenter.stop()
    }

    @Test
    fun `tab tray will get updated if session gets added`() {
        val sessionManager = SessionManager(engine = mock())

        sessionManager.add(Session("https://www.mozilla.org"))
        sessionManager.add(Session("https://getpocket.com"))

        val tabsTray: MockedTabsTray = spy(MockedTabsTray())
        val presenter = TabsTrayPresenter(tabsTray, sessionManager, mock())

        presenter.start()

        assertEquals(2, tabsTray.displaySessionsList!!.size)

        sessionManager.add(Session("https://developer.mozilla.org/"))

        assertEquals(3, tabsTray.updateSessionsList!!.size)

        verify(tabsTray).onSessionsInserted(2, 1)

        presenter.stop()
    }

    @Test
    fun `tabs tray will get updated if session gets removed`() {
        val sessionManager = SessionManager(engine = mock())

        val firstSession = Session("https://www.mozilla.org")
        sessionManager.add(firstSession)
        val secondSession = Session("https://getpocket.com")
        sessionManager.add(secondSession)

        val tabsTray: MockedTabsTray = spy(MockedTabsTray())
        val presenter = TabsTrayPresenter(tabsTray, sessionManager, mock())

        presenter.start()

        assertEquals(2, tabsTray.displaySessionsList!!.size)

        sessionManager.remove(firstSession)
        assertEquals(1, tabsTray.updateSessionsList!!.size)
        verify(tabsTray).onSessionsRemoved(0, 1)

        sessionManager.remove(secondSession)
        assertEquals(0, tabsTray.updateSessionsList!!.size)
        verify(tabsTray, times(2)).onSessionsRemoved(0, 1)

        presenter.stop()
    }

    @Test
    fun `tabs tray will get updated if all sessions get removed`() {
        val sessionManager = SessionManager(engine = mock())

        val firstSession = Session("https://www.mozilla.org")
        sessionManager.add(firstSession)
        sessionManager.add(Session("https://getpocket.com"))

        val tabsTray: MockedTabsTray = spy(MockedTabsTray())
        val presenter = TabsTrayPresenter(tabsTray, sessionManager, mock())

        presenter.start()

        assertEquals(2, tabsTray.displaySessionsList!!.size)

        sessionManager.removeAll()

        assertEquals(0, tabsTray.updateSessionsList!!.size)

        verify(tabsTray).onSessionsRemoved(0, 2)

        presenter.stop()
    }

    @Test
    fun `tabs tray will get updated if selection changes`() {
        val sessionManager = SessionManager(engine = mock())

        sessionManager.add(Session("A"))
        sessionManager.add(Session("B"))
        sessionManager.add(Session("C"))
        val sessionToSelect = Session("D").also { sessionManager.add(it) }
        sessionManager.add(Session("E"))

        val tabsTray: MockedTabsTray = spy(MockedTabsTray())
        val presenter = TabsTrayPresenter(tabsTray, sessionManager, mock())

        presenter.start()

        assertEquals(5, tabsTray.displaySessionsList!!.size)
        assertEquals(0, tabsTray.displaySelectedIndex)

        sessionManager.select(sessionToSelect)

        assertEquals(3, tabsTray.updateSelectedIndex)

        verify(tabsTray).onSessionsChanged(0, 1)
        verify(tabsTray).onSessionsChanged(3, 1)
    }

    @Test
    fun `presenter will close tabs tray and execute callback when all sessions get removed`() {
        val sessionManager = SessionManager(engine = mock())

        sessionManager.add(Session("A"))
        sessionManager.add(Session("B"))

        var closed = false
        var callbackExecuted = false

        val presenter = TabsTrayPresenter(
            mock(),
            sessionManager,
            closeTabsTray = { closed = true },
            onTabsTrayEmpty = { callbackExecuted = true })

        presenter.start()

        assertFalse(closed)
        assertFalse(callbackExecuted)

        sessionManager.removeAll()

        assertTrue(closed)
        assertTrue(callbackExecuted)

        presenter.stop()
    }

    @Test
    fun `presenter will close tabs tray and execute callback when last session gets removed`() {
        val sessionManager = SessionManager(engine = mock())

        val session1 = Session("A").also { sessionManager.add(it) }
        val session2 = Session("B").also { sessionManager.add(it) }

        var closed = false
        var callbackExecuted = false

        val presenter = TabsTrayPresenter(
                mock(),
                sessionManager,
                closeTabsTray = { closed = true },
                onTabsTrayEmpty = { callbackExecuted = true })

        presenter.start()

        assertFalse(closed)
        assertFalse(callbackExecuted)

        sessionManager.remove(session1)

        assertFalse(closed)
        assertFalse(callbackExecuted)

        sessionManager.remove(session2)

        assertTrue(closed)
        assertTrue(callbackExecuted)

        presenter.stop()
    }
}

private class MockedTabsTray : TabsTray {
    var displaySessionsList: List<Session>? = null
    var displaySelectedIndex: Int? = null
    var updateSessionsList: List<Session>? = null
    var updateSelectedIndex: Int? = null

    override fun displaySessions(sessions: List<Session>, selectedIndex: Int) {
        displaySessionsList = sessions
        displaySelectedIndex = selectedIndex
    }

    override fun updateSessions(sessions: List<Session>, selectedIndex: Int) {
        updateSessionsList = sessions
        updateSelectedIndex = selectedIndex
    }

    override fun onSessionsInserted(position: Int, count: Int) {}

    override fun onSessionsRemoved(position: Int, count: Int) {}

    override fun onSessionMoved(fromPosition: Int, toPosition: Int) {}

    override fun onSessionsChanged(position: Int, count: Int) {}

    override fun register(observer: TabsTray.Observer) {}

    override fun register(observer: TabsTray.Observer, owner: LifecycleOwner) {}

    override fun register(observer: TabsTray.Observer, view: View) {}

    override fun unregister(observer: TabsTray.Observer) {}

    override fun unregisterObservers() {}

    override fun notifyObservers(block: TabsTray.Observer.() -> Unit) {}
}
