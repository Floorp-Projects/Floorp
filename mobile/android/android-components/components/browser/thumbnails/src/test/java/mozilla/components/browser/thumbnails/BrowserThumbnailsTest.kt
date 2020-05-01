/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.thumbnails

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineView
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class BrowserThumbnailsTest {

    private lateinit var mockSessionManager: SessionManager
    private lateinit var mockEngineView: EngineView
    private lateinit var thumbnails: BrowserThumbnails

    @Before
    fun setup() {
        val engine = mock<Engine>()
        mockSessionManager = spy(SessionManager(engine))
        mockEngineView = mock()
        thumbnails = BrowserThumbnails(testContext, mockEngineView, mockSessionManager)
    }

    @Test
    fun `when feature is stop must not capture thumbnail when a site finish loading`() {
        thumbnails.start()
        thumbnails.stop()

        val session = getSelectedSession()

        session.notifyObservers {
            onLoadingStateChanged(session, false)
        }

        verify(mockEngineView, never()).captureThumbnail(any())
    }

    @Test
    fun `feature must capture thumbnail when a site finish loading`() {
        thumbnails.start()

        val session = getSelectedSession()

        session.notifyObservers {
            onLoadingStateChanged(session, false)
        }

        verify(mockEngineView).captureThumbnail(any())
    }

    @Test
    fun `when a page is loaded and the os is in low memory condition none thumbnail should be captured`() {
        thumbnails.start()

        val session = getSelectedSession()
        session.thumbnail = mock()

        thumbnails.testLowMemory = true

        session.notifyObservers {
            onLoadingStateChanged(session, false)
        }

        verify(mockEngineView, never()).captureThumbnail(any())
        assertNull(session.thumbnail)
    }

    private fun getSelectedSession(): Session {
        val session = Session("https://www.mozilla.org")
        mockSessionManager.add(session)
        mockSessionManager.select(session)
        return session
    }
}
