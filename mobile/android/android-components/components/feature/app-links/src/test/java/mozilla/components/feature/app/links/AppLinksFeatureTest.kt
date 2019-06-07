/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.feature.app.links

import android.content.Context
import android.content.Intent
import androidx.fragment.app.FragmentManager
import androidx.fragment.app.FragmentTransaction
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.support.test.any
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers
import org.mockito.Mockito
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions
import org.mockito.Mockito.verifyZeroInteractions
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class AppLinksFeatureTest {
    private lateinit var mockContext: Context
    private lateinit var mockSessionManager: SessionManager
    private lateinit var mockFragmentManager: FragmentManager
    private lateinit var mockUseCases: AppLinksUseCases
    private lateinit var mockGetRedirect: AppLinksUseCases.GetAppLinkRedirect
    private lateinit var mockOpenRedirect: AppLinksUseCases.OpenAppLinkRedirect

    private lateinit var feature: AppLinksFeature

    private val webUrl = "https://example.com"
    private val webUrlWithAppLink = "https://soundcloud.com"
    private val intentUrl = "zxing://scan"

    @Before
    fun setup() {
        mockContext = mock(Context::class.java)

        val engine = mock(Engine::class.java)
        mockSessionManager = spy(SessionManager(engine))
        mockFragmentManager = mock(FragmentManager::class.java)
        mockUseCases = mock(AppLinksUseCases::class.java)

        mockGetRedirect = mock(AppLinksUseCases.GetAppLinkRedirect::class.java)
        mockOpenRedirect = mock(AppLinksUseCases.OpenAppLinkRedirect::class.java)
        `when`(mockUseCases.interceptedAppLinkRedirect).thenReturn(mockGetRedirect)
        `when`(mockUseCases.openAppLink).thenReturn(mockOpenRedirect)

        val webRedirect = AppLinkRedirect(null, webUrl, false)
        val appRedirect = AppLinkRedirect(Intent.parseUri(intentUrl, 0), null, false)
        val appRedirectFromWebUrl = AppLinkRedirect(Intent.parseUri(webUrlWithAppLink, 0), null, false)

        `when`(mockGetRedirect.invoke(webUrl)).thenReturn(webRedirect)
        `when`(mockGetRedirect.invoke(intentUrl)).thenReturn(appRedirect)
        `when`(mockGetRedirect.invoke(webUrlWithAppLink)).thenReturn(appRedirectFromWebUrl)

        feature = AppLinksFeature(
            context = mockContext,
            sessionManager = mockSessionManager,
            fragmentManager = mockFragmentManager,
            interceptLinkClicks = true,
            useCases = mockUseCases
        )
    }

    private fun createSession(isPrivate: Boolean): Session {
        val session = mock(Session::class.java)
        `when`(session.private).thenReturn(isPrivate)
        return session
    }

    private fun userTapsOnSession(url: String, private: Boolean) {
        feature.observer.onLoadRequest(
            createSession(private),
            url,
            triggeredByRedirect = false,
            triggeredByWebContent = true
        )
    }

    @Test
    fun `it does not listen for URL changes when it is configured not to`() {
        val subject = AppLinksFeature(
            mockContext,
            mockSessionManager,
            useCases = mockUseCases,
            interceptLinkClicks = false
        )

        subject.start()
        verifyZeroInteractions(mockSessionManager)

        subject.stop()
        verifyZeroInteractions(mockSessionManager)
    }

    @Test
    fun `it tests for app links when triggered by user clicking on a link`() {
        val session = createSession(false)
        val subject = AppLinksFeature(
            mockContext,
            mockSessionManager,
            useCases = mockUseCases
        )

        subject.handleLoadRequest(session, webUrl, true)

        verify(mockGetRedirect).invoke(webUrl)
        verifyNoMoreInteractions(mockOpenRedirect)
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
        `when`(mockSessionManager.findSessionById(ArgumentMatchers.anyString())).thenReturn(mockSession)

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
    fun `an external app is not opened if it does not match`() {
        val mockDialog = spy(RedirectDialogFragment::class.java)

        val featureWithDialog =
            AppLinksFeature(
                context = mockContext,
                sessionManager = mockSessionManager,
                useCases = mockUseCases,
                fragmentManager = mockFragmentManager,
                dialog = mockDialog
            )

        featureWithDialog.start()

        userTapsOnSession(webUrl, true)

        verifyNoMoreInteractions(mockDialog)
        verifyNoMoreInteractions(mockOpenRedirect)
    }

    @Test
    fun `an external app is not opened if it is typed in to the URL bar`() {
        val mockDialog = spy(RedirectDialogFragment::class.java)

        `when`(mockFragmentManager.beginTransaction()).thenReturn(mozilla.components.support.test.mock())

        val featureWithDialog =
            AppLinksFeature(
                context = mockContext,
                sessionManager = mockSessionManager,
                useCases = mockUseCases,
                fragmentManager = mockFragmentManager,
                dialog = mockDialog
            )

        featureWithDialog.start()

        feature.observer.onLoadRequest(
            createSession(true),
            webUrl,
            triggeredByRedirect = false,
            triggeredByWebContent = false
        )

        verifyNoMoreInteractions(mockDialog)
        verifyNoMoreInteractions(mockOpenRedirect)
    }

    @Test
    fun `in non-private mode an external app is opened without a dialog`() {
        val mockDialog = Mockito.spy(RedirectDialogFragment::class.java)
        val mockFragmentManager = mock(FragmentManager::class.java)

        `when`(mockFragmentManager.beginTransaction()).thenReturn(mock(FragmentTransaction::class.java))

        val featureWithDialog =
            AppLinksFeature(
                context = mockContext,
                sessionManager = mockSessionManager,
                useCases = mockUseCases,
                fragmentManager = mockFragmentManager,
                dialog = mockDialog
            )

        featureWithDialog.start()

        userTapsOnSession(intentUrl, false)

        verifyNoMoreInteractions(mockDialog)
        verify(mockOpenRedirect).invoke(any())
    }
}
