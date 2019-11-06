/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.app.links

import android.content.Context
import android.content.Intent
import android.net.Uri
import androidx.fragment.app.FragmentManager
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.session.LegacySessionManager
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers
import org.mockito.Mockito.`when`
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions
import org.mockito.Mockito.verifyZeroInteractions

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
        mockUseCases = mock()
        mockEngineSession = mock()

        mockGetRedirect = mock()
        mockOpenRedirect = mock()
        `when`(mockUseCases.interceptedAppLinkRedirect).thenReturn(mockGetRedirect)
        `when`(mockUseCases.openAppLink).thenReturn(mockOpenRedirect)

        val webRedirect = AppLinkRedirect(null, webUrl)
        val appRedirect = AppLinkRedirect(Intent.parseUri(intentUrl, 0), null)
        val appRedirectFromWebUrl = AppLinkRedirect(Intent.parseUri(webUrlWithAppLink, 0), null)

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

    private fun createSession(isPrivate: Boolean, url: String = "https://mozilla.com"): Session {
        val session = mock<Session>()
        `when`(session.private).thenReturn(isPrivate)
        `when`(session.url).thenReturn(url)
        return session
    }

    private fun userTapsOnSession(url: String, private: Boolean): Boolean {
        return feature.observer.onLoadRequest(
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
            interceptLinkClicks = false,
            alwaysAllowedSchemes = setOf()
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

        subject.handleLoadRequest(session, webUrlWithAppLink, true)

        verify(mockGetRedirect).invoke(webUrlWithAppLink)
        verifyNoMoreInteractions(mockOpenRedirect)
    }

    @Test
    fun `it tests for white and black listed schemes when triggered by user clicking on a link`() {
        val whitelistedScheme = "whitelisted"
        val blacklistedScheme = "blacklisted"
        val session = createSession(false)
        val subject = AppLinksFeature(
            mockContext,
            mockSessionManager,
            interceptLinkClicks = false,
            alwaysAllowedSchemes = setOf(whitelistedScheme),
            alwaysDeniedSchemes = setOf(blacklistedScheme),
            useCases = mockUseCases
        )

        val blackListedUrl = "$blacklistedScheme://example.com"
        val blacklistedRedirect = AppLinkRedirect(Intent.parseUri(blackListedUrl, 0), blackListedUrl)
        `when`(mockGetRedirect.invoke(blackListedUrl)).thenReturn(blacklistedRedirect)
        subject.handleLoadRequest(session, blackListedUrl, true)
        verify(mockGetRedirect).invoke(blackListedUrl)
        verify(mockOpenRedirect, never()).invoke(blacklistedRedirect)

        val whiteListedUrl = "$whitelistedScheme://example.com"
        val whitelistedRedirect = AppLinkRedirect(Intent.parseUri(whiteListedUrl, 0), whiteListedUrl)
        `when`(mockGetRedirect.invoke(whiteListedUrl)).thenReturn(whitelistedRedirect)
        subject.handleLoadRequest(session, whiteListedUrl, true)
        verify(mockGetRedirect).invoke(whiteListedUrl)
        verify(mockOpenRedirect).invoke(whitelistedRedirect)
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
        `when`(mockLegacySessionManager.findSessionById(ArgumentMatchers.anyString())).thenReturn(mockSession)

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
    fun `when a whitelist of schemes is provided, observe the selected session`() {
        feature = AppLinksFeature(
            mockContext,
            sessionManager = mockSessionManager,
            useCases = mockUseCases,
            interceptLinkClicks = false,
            alwaysAllowedSchemes = setOf("whitelisted")
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
            spy(AppLinksFeature(
                context = mockContext,
                sessionManager = mockSessionManager,
                useCases = mockUseCases,
                fragmentManager = mockFragmentManager,
                dialog = mockDialog
            ))

        `when`(mockLegacySessionManager.getOrCreateEngineSession(any())).thenReturn(mockEngineSession)
        featureWithDialog.start()

        val consumed = userTapsOnSession(webUrl, true)

        assertEquals(true, consumed)
        verifyNoMoreInteractions(mockDialog)
        verifyNoMoreInteractions(mockOpenRedirect)
    }

    @Test
    fun `an external app is not opened if it is typed in to the URL bar`() {
        val mockDialog = spy(RedirectDialogFragment::class.java)
        `when`(mockFragmentManager.beginTransaction()).thenReturn(mock())

        val featureWithDialog =
            AppLinksFeature(
                context = mockContext,
                sessionManager = mockSessionManager,
                useCases = mockUseCases,
                fragmentManager = mockFragmentManager,
                dialog = mockDialog
            )

        featureWithDialog.start()

        val consumed = feature.observer.onLoadRequest(
            createSession(true),
            webUrl,
            triggeredByRedirect = false,
            triggeredByWebContent = false
        )

        assertEquals(false, consumed)
        verifyNoMoreInteractions(mockDialog)
        verifyNoMoreInteractions(mockOpenRedirect)
    }

    @Test
    fun `an external app is not opened if the current session is already on the same host`() {
        val mockDialog = spy(RedirectDialogFragment::class.java)

        `when`(mockFragmentManager.beginTransaction()).thenReturn(mock())

        val feature =
            AppLinksFeature(
                context = mockContext,
                sessionManager = mockSessionManager,
                useCases = mockUseCases,
                fragmentManager = mockFragmentManager,
                dialog = mockDialog
            )

        feature.start()

        this.feature.observer.onLoadRequest(
            createSession(true, webUrl),
            "$webUrl/backButton",
            triggeredByRedirect = false,
            triggeredByWebContent = false
        )

        verifyNoMoreInteractions(mockDialog)
        verifyNoMoreInteractions(mockOpenRedirect)
    }

    @Test
    fun `in non-private mode an external app is opened without a dialog`() {
        val mockDialog = spy(RedirectDialogFragment::class.java)
        val mockFragmentManager = mock<FragmentManager>()
        `when`(mockFragmentManager.beginTransaction()).thenReturn(mock())

        val featureWithDialog =
            AppLinksFeature(
                context = mockContext,
                sessionManager = mockSessionManager,
                useCases = mockUseCases,
                fragmentManager = mockFragmentManager,
                dialog = mockDialog
            )

        featureWithDialog.start()

        val consumed = userTapsOnSession(intentUrl, false)

        assertEquals(true, consumed)
        verifyNoMoreInteractions(mockDialog)
        verify(mockOpenRedirect).invoke(any())
    }

    @Test
    fun `an external app is not opened for URLs with javascript scheme`() {
        val javascriptUri = "javascript:;"

        val openAppUseCase: AppLinksUseCases.OpenAppLinkRedirect = mock()
        val useCases: AppLinksUseCases = mock()
        whenever(useCases.openAppLink).thenReturn(openAppUseCase)

        val feature = AppLinksFeature(
            testContext,
            sessionManager = mock(),
            interceptLinkClicks = true,
            useCases = useCases
        )

        val intent = Intent().apply {
            data = Uri.parse(javascriptUri)
        }

        val redirect = AppLinkRedirect(
            intent,
            javascriptUri)

        feature.handleRedirect(redirect, Session("https://www.amazon.ca"))

        verify(openAppUseCase, never()).invoke(redirect)
    }

    @Test
    fun `Use the fallback URL when app is not installed`() {
        val feature = spy(AppLinksFeature(
                testContext,
                sessionManager = mockSessionManager,
                interceptLinkClicks = true,
                useCases = mockUseCases
        ))

        val redirect = AppLinkRedirect(null, webUrl)
        val session = Session(webUrl)

        `when`(mockLegacySessionManager.getOrCreateEngineSession(any())).thenReturn(mockEngineSession)
        feature.handleRedirect(redirect, session)

        verify(feature).handleFallback(redirect, session)
    }
}
