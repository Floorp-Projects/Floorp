/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.app.links

import android.content.Context
import android.content.Intent
import androidx.fragment.app.FragmentManager
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.session.LegacySessionManager
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.ArgumentMatchers.anyString
import org.mockito.ArgumentMatchers.eq
import org.mockito.Mockito.`when`
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions

@RunWith(AndroidJUnit4::class)
class AppLinksFeatureTest {

    private lateinit var mockContext: Context
    private lateinit var mockSessionManager: SessionManager
    private lateinit var mockFragmentManager: FragmentManager
    private lateinit var mockUseCases: AppLinksUseCases
    private lateinit var mockGetRedirect: AppLinksUseCases.GetAppLinkRedirect
    private lateinit var mockOpenRedirect: AppLinksUseCases.OpenAppLinkRedirect
    private lateinit var mockEngineSession: EngineSession
    private lateinit var mockLegacySessionManager: LegacySessionManager
    private lateinit var mockDialog: RedirectDialogFragment

    private lateinit var feature: AppLinksFeature

    private val webUrl = "https://example.com"
    private val webUrlWithAppLink = "https://soundcloud.com"
    private val intentUrl = "zxing://scan"

    @Before
    fun setup() {
        mockContext = mock()

        val engine = mock<Engine>()
        mockLegacySessionManager = mock()
        mockSessionManager = spy(SessionManager(engine, delegate = mockLegacySessionManager))
        mockFragmentManager = mock()
        `when`(mockFragmentManager.beginTransaction()).thenReturn(mock())
        mockUseCases = mock()
        mockEngineSession = mock()
        mockDialog = mock()

        mockGetRedirect = mock()
        mockOpenRedirect = mock()
        `when`(mockUseCases.interceptedAppLinkRedirect).thenReturn(mockGetRedirect)
        `when`(mockUseCases.openAppLink).thenReturn(mockOpenRedirect)

        val webRedirect = AppLinkRedirect(null, webUrl, null)
        val appRedirect = AppLinkRedirect(Intent.parseUri(intentUrl, 0), null, null)
        val appRedirectFromWebUrl = AppLinkRedirect(Intent.parseUri(webUrlWithAppLink, 0), null, null)

        `when`(mockGetRedirect.invoke(webUrl)).thenReturn(webRedirect)
        `when`(mockGetRedirect.invoke(intentUrl)).thenReturn(appRedirect)
        `when`(mockGetRedirect.invoke(webUrlWithAppLink)).thenReturn(appRedirectFromWebUrl)

        feature = AppLinksFeature(
            context = mockContext,
            sessionManager = mockSessionManager,
            fragmentManager = mockFragmentManager,
            useCases = mockUseCases,
            dialog = mockDialog
        )
    }

    private fun createSession(isPrivate: Boolean, url: String = "https://mozilla.com"): Session {
        val session = mock<Session>()
        `when`(session.private).thenReturn(isPrivate)
        `when`(session.url).thenReturn(url)
        return session
    }

    private fun userTapsOnSession(url: String, private: Boolean) {
        feature.observer.onLaunchIntentRequest(
            createSession(private),
            url = url,
            appIntent = mock()
        )
    }

    @Test
    fun `when valid sessionId is provided, observe its session`() {
        feature = AppLinksFeature(
            mockContext,
            sessionManager = mockSessionManager,
            sessionId = "123",
            useCases = mockUseCases
        )
        val mockSession = createSession(false)
        `when`(mockLegacySessionManager.findSessionById(anyString())).thenReturn(mockSession)

        feature.start()

        verify(mockSession).register(feature.observer)
    }

    @Test
    fun `when sessionId is NOT provided, observe selected session`() {
        feature = AppLinksFeature(
            mockContext,
            sessionManager = mockSessionManager,
            useCases = mockUseCases
        )

        feature.start()

        verify(mockSessionManager).register(feature.observer)
    }

    @Test
    fun `when start is called must register SessionManager observers`() {
        feature.start()
        verify(mockSessionManager).register(feature.observer)
    }

    @Test
    fun `when stop is called must unregister SessionManager observers `() {
        feature.stop()
        verify(mockSessionManager).unregister(feature.observer)
    }

    @Test
    fun `in non-private mode an external app is opened without a dialog`() {
        feature.start()
        userTapsOnSession(intentUrl, false)

        verifyNoMoreInteractions(mockDialog)
        verify(mockOpenRedirect).invoke(any(), anyBoolean(), any())
    }

    @Test
    fun `in private mode an external app dialog is shown`() {
        feature.start()
        userTapsOnSession(intentUrl, true)

        verify(mockDialog).show(eq(mockFragmentManager), anyString())
        verify(mockOpenRedirect, never()).invoke(any(), anyBoolean(), any())
    }

    @Test
    fun `reused redirect dialog if exists`() {
        feature.start()
        userTapsOnSession(intentUrl, true)

        val dialog = feature.getOrCreateDialog()
        assertEquals(dialog, feature.getOrCreateDialog())
    }
}
