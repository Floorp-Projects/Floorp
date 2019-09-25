/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session

import android.graphics.Bitmap
import mozilla.components.concept.engine.HitResult
import mozilla.components.concept.engine.content.blocking.Tracker
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito

class SelectionAwareSessionObserverTest {
    private lateinit var sessionManager: SessionManager
    private lateinit var session1: Session
    private lateinit var session2: Session
    private lateinit var session3: Session
    private lateinit var observer: MockObserver

    @Before
    fun setup() {
        sessionManager = SessionManager(engine = mock())
        session1 = Session("https://mozilla.org")
        session2 = Session("https://getpocket.com")
        session3 = Session("https://wiki.mozilla.org", id = "123")
        observer = MockObserver(sessionManager)
    }

    @Test
    fun `observe specific session`() {
        observer.observeFixed(session1)
        assertEquals(session1, observer.activeSession)

        session1.url = "changed"
        assertEquals("changed", observer.observedUrl)
    }

    @Test
    fun `observe selected session`() {
        sessionManager.add(session1, selected = true)
        observer.observeSelected()
        assertEquals(session1, observer.activeSession)

        session1.url = "changed"
        assertEquals("changed", observer.observedUrl)
    }

    @Test
    fun `observe the session based on the provided ID if exists or the selected session`() {
        sessionManager.add(session1)
        observer.observeIdOrSelected("123")
        assertEquals(session1, observer.activeSession)

        sessionManager.add(session2, selected = true)
        observer.observeIdOrSelected("123")
        assertEquals(session2, observer.activeSession)

        sessionManager.add(session3)
        observer.observeIdOrSelected("123")
        assertEquals(session3, observer.activeSession)

        observer.observeIdOrSelected(null)
        assertEquals(session2, observer.activeSession)
    }

    @Test
    fun `observe selected session on empty session manager`() {
        observer.observeSelected()
        assertNull(observer.activeSession)

        sessionManager.add(session1)
        assertEquals(session1, observer.activeSession)

        session1.url = "changed"
        assertEquals("changed", observer.observedUrl)
    }

    @Test
    fun `observer handles session selection change`() {
        sessionManager.add(session1, selected = true)
        sessionManager.add(session2)

        observer.observeSelected()
        assertEquals(session1, observer.activeSession)

        sessionManager.select(session2)
        assertEquals(session2, observer.activeSession)
    }

    @Test
    fun `observer notified of changes to selected session only`() {
        sessionManager.add(session1, selected = true)
        sessionManager.add(session2)

        observer.observeSelected()

        session1.url = "changed1"
        session2.url = "changed2"
        assertEquals("changed1", observer.observedUrl)

        session1.url = "http://mozilla.org"
        session2.url = "http://getpocket.org"

        sessionManager.select(session2)
        session2.url = "changed2"
        session1.url = "changed1"
        assertEquals("changed2", observer.observedUrl)
    }

    @Test
    fun `observer not notified after stop was called`() {
        sessionManager.add(session1, selected = true)
        sessionManager.add(session2)

        observer.observeSelected()
        assertEquals(session1, observer.activeSession)

        observer.stop()
        sessionManager.select(session2)
        assertEquals(session1, observer.activeSession)

        session1.url = "changed1"
        assertEquals("", observer.observedUrl)
    }

    @Test
    fun `stop has no effect if observe was never called`() {
        assertNull(observer.activeSession)
        observer.stop()
        assertNull(observer.activeSession)
    }

    @Test
    fun `observer has default methods`() {
        val session = Session("")
        val observer = object : SelectionAwareSessionObserver(sessionManager) { }
        observer.onUrlChanged(session, "")
        observer.onTitleChanged(session, "")
        observer.onProgress(session, 0)
        observer.onLoadingStateChanged(session, true)
        observer.onNavigationStateChanged(session, true, true)
        observer.onSearch(session, "")
        observer.onSecurityChanged(session, Session.SecurityInfo())
        observer.onCustomTabConfigChanged(session, null)
        observer.onTrackerBlockingEnabledChanged(session, true)
        observer.onTrackerBlocked(session, Tracker(""), emptyList())
        observer.onLongPress(session, Mockito.mock(HitResult::class.java))
        observer.onFindResult(session, Mockito.mock(Session.FindResult::class.java))
        observer.onDesktopModeChanged(session, true)
        observer.onFullScreenChanged(session, true)
        observer.onThumbnailChanged(session, Mockito.spy(Bitmap::class.java))
        observer.onSessionRemoved(session)
        observer.onAllSessionsRemoved()
    }

    class MockObserver(sessionManager: SessionManager) : SelectionAwareSessionObserver(sessionManager) {
        public override var activeSession: Session?
            get() = super.activeSession
            set(value) { super.activeSession = value }

        var observedUrl: String = ""

        override fun onUrlChanged(session: Session, url: String) {
            observedUrl = url
        }
    }
}
