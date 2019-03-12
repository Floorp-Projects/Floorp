/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import android.content.Context
import android.graphics.Bitmap
import androidx.test.core.app.ApplicationProvider
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineView
import mozilla.components.support.test.any
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class ThumbnailsFeatureTest {

    private lateinit var mockSessionManager: SessionManager
    private lateinit var mockEngineView: EngineView
    private lateinit var feature: ThumbnailsFeature

    @Before
    fun setup() {
        val engine = mock(Engine::class.java)
        val context = ApplicationProvider.getApplicationContext<Context>()
        mockSessionManager = spy(SessionManager(engine))
        mockEngineView = mock(EngineView::class.java)
        feature = ThumbnailsFeature(context, mockEngineView, mockSessionManager)
    }

    @Test
    fun `when feature is stop must not capture thumbnail when a site finish loading`() {
        feature.start()
        feature.stop()

        val session = getSelectedSession()

        session.notifyObservers {
            onLoadingStateChanged(session, false)
        }

        verify(mockEngineView, never()).captureThumbnail(any())
    }

    @Test
    fun `feature must capture thumbnail when a site finish loading`() {
        feature.start()

        val session = getSelectedSession()

        session.notifyObservers {
            onLoadingStateChanged(session, false)
        }

        verify(mockEngineView).captureThumbnail(any())
    }

    @Test
    fun `when a page is loaded and the os is in low memory condition none thumbnail should be captured`() {
        feature.start()

        val session = getSelectedSession()
        session.thumbnail = mock(Bitmap::class.java)

        feature.testLowMemory = true

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
