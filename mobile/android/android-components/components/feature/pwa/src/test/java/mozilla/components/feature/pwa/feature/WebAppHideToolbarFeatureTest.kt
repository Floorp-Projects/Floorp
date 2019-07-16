/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.feature

import android.view.View
import androidx.core.net.toUri
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class WebAppHideToolbarFeatureTest {

    @Test
    fun `hides toolbar immediately`() {
        val toolbar: View = mock()

        WebAppHideToolbarFeature(mock(), toolbar, "id", listOf(mock()))
        verify(toolbar).visibility = View.GONE

        WebAppHideToolbarFeature(mock(), toolbar, "id", emptyList())
        verify(toolbar).visibility = View.VISIBLE
    }

    @Test
    fun `registers session observer`() {
        val sessionManager: SessionManager = mock()
        val session: Session = mock()
        `when`(sessionManager.findSessionById("id")).thenReturn(session)

        WebAppHideToolbarFeature(sessionManager, mock(), "id", emptyList()).start()
        verify(session, never()).register(any())

        val feature = WebAppHideToolbarFeature(sessionManager, mock(), "id", listOf(mock()))

        feature.start()
        verify(session).register(feature)

        feature.stop()
        verify(session).unregister(feature)
    }

    @Test
    fun `hides toolbar if URL is in origin`() {
        val trusted = listOf("https://mozilla.com".toUri(), "https://m.mozilla.com".toUri())
        val toolbar = View(testContext)
        val feature = WebAppHideToolbarFeature(mock(), toolbar, "id", trusted)

        feature.onUrlChanged(mock(), "https://mozilla.com/example-page")
        assertEquals(View.GONE, toolbar.visibility)

        feature.onUrlChanged(mock(), "https://firefox.com/out-of-scope")
        assertEquals(View.VISIBLE, toolbar.visibility)

        feature.onUrlChanged(mock(), "https://mozilla.com/back-in-scope")
        assertEquals(View.GONE, toolbar.visibility)

        feature.onUrlChanged(mock(), "https://m.mozilla.com/second-origin")
        assertEquals(View.GONE, toolbar.visibility)
    }

    @Test
    fun `hides toolbar if URL is in scope`() {
        val trusted = listOf("https://mozilla.github.io/my-app/".toUri())
        val toolbar = View(testContext)
        val feature = WebAppHideToolbarFeature(mock(), toolbar, "id", trusted)

        feature.onUrlChanged(mock(), "https://mozilla.github.io/my-app/")
        assertEquals(View.GONE, toolbar.visibility)

        feature.onUrlChanged(mock(), "https://firefox.com/out-of-scope")
        assertEquals(View.VISIBLE, toolbar.visibility)

        feature.onUrlChanged(mock(), "https://mozilla.github.io/my-app-almost-in-scope")
        assertEquals(View.VISIBLE, toolbar.visibility)

        feature.onUrlChanged(mock(), "https://mozilla.github.io/my-app/sub-page")
        assertEquals(View.GONE, toolbar.visibility)
    }

    @Test
    fun `hides toolbar if URL is in ambiguous scope`() {
        val trusted = listOf("https://mozilla.github.io/prefix".toUri())
        val toolbar = View(testContext)
        val feature = WebAppHideToolbarFeature(mock(), toolbar, "id", trusted)

        feature.onUrlChanged(mock(), "https://mozilla.github.io/prefix/")
        assertEquals(View.GONE, toolbar.visibility)

        feature.onUrlChanged(mock(), "https://mozilla.github.io/prefix-of/resource.html")
        assertEquals(View.GONE, toolbar.visibility)
    }
}
