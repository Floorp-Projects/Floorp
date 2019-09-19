/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa.feature

import android.view.View
import androidx.core.net.toUri
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class WebAppHideToolbarFeatureTest {

    @Test
    fun `hides toolbar immediately`() {
        val toolbar: View = mock()
        var changeResult = true

        WebAppHideToolbarFeature(mock(), toolbar, "id", listOf(mock())) { changeResult = it }
        verify(toolbar).visibility = View.GONE
        assertFalse(changeResult)

        WebAppHideToolbarFeature(mock(), toolbar, "id", emptyList()) { changeResult = it }
        verify(toolbar).visibility = View.VISIBLE
        assertTrue(changeResult)
    }

    @Test
    fun `registers session observer`() {
        val sessionManager: SessionManager = mock()
        val session: Session = mock()
        `when`(sessionManager.findSessionById("id")).thenReturn(session)

        val feature = WebAppHideToolbarFeature(sessionManager, mock(), "id", listOf(mock()))

        feature.start()
        verify(session).register(feature)

        feature.stop()
        verify(session).unregister(feature)
    }

    @Test
    fun `onUrlChanged hides toolbar if URL is in origin`() {
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
    fun `onUrlChanged hides toolbar if URL is in scope`() {
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
    fun `onUrlChanged hides toolbar if URL is in ambiguous scope`() {
        val trusted = listOf("https://mozilla.github.io/prefix".toUri())
        val toolbar = View(testContext)
        val feature = WebAppHideToolbarFeature(mock(), toolbar, "id", trusted)

        feature.onUrlChanged(mock(), "https://mozilla.github.io/prefix/")
        assertEquals(View.GONE, toolbar.visibility)

        feature.onUrlChanged(mock(), "https://mozilla.github.io/prefix-of/resource.html")
        assertEquals(View.GONE, toolbar.visibility)
    }

    @Test
    fun `onTrustedScopesChange hides toolbar if URL is in origin`() {
        val sessionManager: SessionManager = mock()
        val session: Session = mock()
        val trusted = listOf("https://mozilla.com".toUri(), "https://m.mozilla.com".toUri())
        val toolbar = View(testContext)
        val feature = WebAppHideToolbarFeature(sessionManager, toolbar, "id", trusted)

        doReturn(session).`when`(sessionManager).findSessionById("id")
        doReturn("https://mozilla.com/example-page").`when`(session).url

        feature.onTrustedScopesChange(listOf("https://m.mozilla.com".toUri()))
        assertEquals(View.VISIBLE, toolbar.visibility)

        feature.onTrustedScopesChange(listOf("https://mozilla.com".toUri()))
        assertEquals(View.GONE, toolbar.visibility)
    }

    @Test
    fun `onTrustedScopesChange hides toolbar if URL is in scope`() {
        val sessionManager: SessionManager = mock()
        val session: Session = mock()
        val trusted = listOf("https://mozilla.github.io/my-app/".toUri())
        val toolbar = View(testContext)
        val feature = WebAppHideToolbarFeature(sessionManager, toolbar, "id", trusted)

        doReturn(session).`when`(sessionManager).findSessionById("id")
        doReturn("https://mozilla.github.io/my-app/").`when`(session).url

        feature.onTrustedScopesChange(listOf("https://mozilla.github.io/my-app/".toUri()))
        assertEquals(View.GONE, toolbar.visibility)

        feature.onTrustedScopesChange(listOf("https://firefox.com/out-of-scope/".toUri()))
        assertEquals(View.VISIBLE, toolbar.visibility)

        feature.onTrustedScopesChange(listOf("https://mozilla.github.io/my-app-almost-in-scope".toUri()))
        assertEquals(View.VISIBLE, toolbar.visibility)
    }

    @Test
    fun `onTrustedScopesChange hides toolbar if URL is in ambiguous scope`() {
        val sessionManager: SessionManager = mock()
        val session: Session = mock()
        val trusted = listOf("https://mozilla.github.io/prefix".toUri())
        val toolbar = View(testContext)
        val feature = WebAppHideToolbarFeature(sessionManager, toolbar, "id", trusted)

        doReturn(session).`when`(sessionManager).findSessionById("id")
        doReturn("https://mozilla.github.io/prefix-of/resource.html").`when`(session).url

        feature.onTrustedScopesChange(listOf("https://mozilla.github.io/prefix".toUri()))
        assertEquals(View.GONE, toolbar.visibility)

        feature.onTrustedScopesChange(listOf("https://mozilla.github.io/prefix-of/".toUri()))
        assertEquals(View.GONE, toolbar.visibility)
    }
}
