/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.app.links

import android.content.Context
import android.content.Intent
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.request.RequestInterceptor
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class AppLinksInterceptorTest {
    private lateinit var mockContext: Context
    private lateinit var mockUseCases: AppLinksUseCases
    private lateinit var mockGetRedirect: AppLinksUseCases.GetAppLinkRedirect
    private lateinit var mockEngineSession: EngineSession
    private lateinit var mockOpenRedirect: AppLinksUseCases.OpenAppLinkRedirect

    private lateinit var appLinksInterceptor: AppLinksInterceptor

    private val webUrl = "https://example.com"
    private val webUrlWithAppLink = "https://soundcloud.com"
    private val intentUrl = "zxing://scan;S.browser_fallback_url=example.com"
    private val fallbackUrl = "https://getpocket.com"
    private val marketplaceUrl = "market://details?id=example.com"

    @Before
    fun setup() {
        mockContext = mock()
        mockUseCases = mock()
        mockEngineSession = mock()
        mockGetRedirect = mock()
        mockOpenRedirect = mock()
        whenever(mockUseCases.interceptedAppLinkRedirect).thenReturn(mockGetRedirect)
        whenever(mockUseCases.openAppLink).thenReturn(mockOpenRedirect)

        val webRedirect = AppLinkRedirect(null, webUrl, null)
        val appRedirect = AppLinkRedirect(Intent.parseUri(intentUrl, 0), null, null)
        val appRedirectFromWebUrl = AppLinkRedirect(Intent.parseUri(webUrlWithAppLink, 0), null, null)
        val fallbackRedirect = AppLinkRedirect(null, fallbackUrl, null)
        val marketRedirect = AppLinkRedirect(null, null, Intent.parseUri(marketplaceUrl, 0))

        whenever(mockGetRedirect.invoke(webUrl)).thenReturn(webRedirect)
        whenever(mockGetRedirect.invoke(intentUrl)).thenReturn(appRedirect)
        whenever(mockGetRedirect.invoke(webUrlWithAppLink)).thenReturn(appRedirectFromWebUrl)
        whenever(mockGetRedirect.invoke(fallbackUrl)).thenReturn(fallbackRedirect)
        whenever(mockGetRedirect.invoke(marketplaceUrl)).thenReturn(marketRedirect)

        appLinksInterceptor = AppLinksInterceptor(
            context = mockContext,
            interceptLinkClicks = true,
            launchInApp = { true },
            useCases = mockUseCases,
        )
    }

    @Test
    fun `request is intercepted by user clicking on a link`() {
        val response = appLinksInterceptor.onLoadRequest(mockEngineSession, webUrlWithAppLink, null, true, false, false, false, false)
        assert(response is RequestInterceptor.InterceptionResponse.AppIntent)
    }

    @Test
    fun `request is intercepted by redirect`() {
        val response = appLinksInterceptor.onLoadRequest(mockEngineSession, webUrlWithAppLink, null, false, false, true, false, false)
        assert(response is RequestInterceptor.InterceptionResponse.AppIntent)
    }

    @Test
    fun `request is not intercepted by a subframe redirect`() {
        val response = appLinksInterceptor.onLoadRequest(mockEngineSession, webUrl, null, false, false, true, false, true)
        assertEquals(null, response)
    }

    @Test
    fun `request is intercepted by direct navigation`() {
        val response = appLinksInterceptor.onLoadRequest(mockEngineSession, webUrlWithAppLink, null, false, false, false, true, false)
        assert(response is RequestInterceptor.InterceptionResponse.AppIntent)
    }

    @Test
    fun `request is not intercepted when interceptLinkClicks is false`() {
        appLinksInterceptor = AppLinksInterceptor(
            context = mockContext,
            interceptLinkClicks = false,
            launchInApp = { true },
            useCases = mockUseCases,
        )

        val response = appLinksInterceptor.onLoadRequest(mockEngineSession, webUrlWithAppLink, null, true, false, false, false, false)
        assertEquals(null, response)
    }

    @Test
    fun `request is not intercepted when launchInApp preference is false`() {
        appLinksInterceptor = AppLinksInterceptor(
            context = mockContext,
            interceptLinkClicks = true,
            launchInApp = { false },
            useCases = mockUseCases,
        )

        val response = appLinksInterceptor.onLoadRequest(mockEngineSession, webUrlWithAppLink, null, true, false, false, false, false)
        assertEquals(null, response)
    }

    @Test
    fun `request is not intercepted when not user clicking on a link`() {
        val response = appLinksInterceptor.onLoadRequest(mockEngineSession, webUrlWithAppLink, null, false, false, false, false, false)
        assertEquals(null, response)
    }

    @Test
    fun `request is not intercepted if the current session is already on the same host`() {
        val response = appLinksInterceptor.onLoadRequest(mockEngineSession, webUrlWithAppLink, webUrlWithAppLink, true, true, false, false, false)
        assertEquals(null, response)
    }

    @Test
    fun `request is not intercepted by a redirect on same domain`() {
        val response = appLinksInterceptor.onLoadRequest(mockEngineSession, webUrlWithAppLink, webUrlWithAppLink, true, true, true, false, false)
        assertEquals(null, response)
    }

    @Test
    fun `domain is stripped before checking`() {
        var response = appLinksInterceptor.onLoadRequest(mockEngineSession, "http://example.com", "example.com", true, true, true, false, false)
        assertEquals(null, response)

        response = appLinksInterceptor.onLoadRequest(mockEngineSession, "https://example.com", "http://example.com", true, true, true, false, false)
        assertEquals(null, response)

        response = appLinksInterceptor.onLoadRequest(mockEngineSession, "https://www.example.com", "http://example.com", true, true, true, false, false)
        assertEquals(null, response)

        response = appLinksInterceptor.onLoadRequest(mockEngineSession, "http://www.example.com", "https://www.example.com", true, true, true, false, false)
        assertEquals(null, response)

        response = appLinksInterceptor.onLoadRequest(mockEngineSession, "http://m.example.com", "https://www.example.com", true, true, true, false, false)
        assertEquals(null, response)

        response = appLinksInterceptor.onLoadRequest(mockEngineSession, "http://mobile.example.com", "http://m.example.com", true, true, true, false, false)
        assertEquals(null, response)
    }

    @Test
    fun `request is not intercepted if a subframe request and not triggered by user`() {
        val response = appLinksInterceptor.onLoadRequest(mockEngineSession, webUrlWithAppLink, null, false, false, false, true, true)
        assertEquals(null, response)
    }

    @Test
    fun `request is not intercepted if not user gesture, not redirect and not direct navigation`() {
        val response = appLinksInterceptor.onLoadRequest(mockEngineSession, webUrlWithAppLink, null, false, false, false, false, false)
        assertEquals(null, response)
    }

    @Test
    fun `block listed schemes request not intercepted when triggered by user clicking on a link`() {
        val engineSession: EngineSession = mock()
        val blocklistedScheme = "blocklisted"
        val feature = AppLinksInterceptor(
            context = mockContext,
            interceptLinkClicks = true,
            alwaysDeniedSchemes = setOf(blocklistedScheme),
            launchInApp = { true },
            useCases = mockUseCases,
        )

        val blocklistedUrl = "$blocklistedScheme://example.com"
        val blocklistedRedirect = AppLinkRedirect(Intent.parseUri(blocklistedUrl, 0), blocklistedUrl, null)
        whenever(mockGetRedirect.invoke(blocklistedUrl)).thenReturn(blocklistedRedirect)
        var response = feature.onLoadRequest(engineSession, blocklistedUrl, null, true, false, false, false, false)
        assertEquals(null, response)
    }

    @Test
    fun `supported schemes request not launched if launchInApp is false`() {
        val engineSession: EngineSession = mock()
        val supportedScheme = "supported"
        val feature = AppLinksInterceptor(
            context = mockContext,
            interceptLinkClicks = true,
            engineSupportedSchemes = setOf(supportedScheme),
            launchInApp = { false },
            useCases = mockUseCases,
        )

        val supportedUrl = "$supportedScheme://example.com"
        val supportedRedirect = AppLinkRedirect(Intent.parseUri(supportedUrl, 0), null, null)
        whenever(mockGetRedirect.invoke(supportedUrl)).thenReturn(supportedRedirect)
        val response = feature.onLoadRequest(engineSession, supportedUrl, null, true, false, false, false, false)
        assertEquals(null, response)
    }

    @Test
    fun `supported schemes request not launched if interceptLinkClicks is false`() {
        val engineSession: EngineSession = mock()
        val supportedScheme = "supported"
        val feature = AppLinksInterceptor(
            context = mockContext,
            interceptLinkClicks = false,
            engineSupportedSchemes = setOf(supportedScheme),
            launchInApp = { true },
            useCases = mockUseCases,
        )

        val supportedUrl = "$supportedScheme://example.com"
        val supportedRedirect = AppLinkRedirect(Intent.parseUri(supportedUrl, 0), null, null)
        whenever(mockGetRedirect.invoke(supportedUrl)).thenReturn(supportedRedirect)
        val response = feature.onLoadRequest(engineSession, supportedUrl, null, true, false, false, false, false)
        assertEquals(null, response)
    }

    @Test
    fun `supported schemes request not launched if not triggered by user`() {
        val engineSession: EngineSession = mock()
        val supportedScheme = "supported"
        val feature = AppLinksInterceptor(
            context = mockContext,
            interceptLinkClicks = true,
            engineSupportedSchemes = setOf(supportedScheme),
            launchInApp = { true },
            useCases = mockUseCases,
        )

        val supportedUrl = "$supportedScheme://example.com"
        val supportedRedirect = AppLinkRedirect(Intent.parseUri(supportedUrl, 0), null, null)
        whenever(mockGetRedirect.invoke(supportedUrl)).thenReturn(supportedRedirect)
        val response = feature.onLoadRequest(engineSession, supportedUrl, null, false, false, false, false, false)
        assertEquals(null, response)
    }

    @Test
    fun `not supported schemes request always intercepted regardless of hasUserGesture, interceptLinkClicks or launchInApp`() {
        val engineSession: EngineSession = mock()
        val supportedScheme = "supported"
        val notSupportedScheme = "not_supported"
        val blocklistedScheme = "blocklisted"
        val feature = AppLinksInterceptor(
            context = mockContext,
            interceptLinkClicks = false,
            engineSupportedSchemes = setOf(supportedScheme),
            alwaysDeniedSchemes = setOf(blocklistedScheme),
            launchInApp = { false },
            useCases = mockUseCases,
        )

        val notSupportedUrl = "$notSupportedScheme://example.com"
        val notSupportedRedirect = AppLinkRedirect(Intent.parseUri(notSupportedUrl, 0), null, null)
        whenever(mockGetRedirect.invoke(notSupportedUrl)).thenReturn(notSupportedRedirect)
        val response = feature.onLoadRequest(engineSession, notSupportedUrl, null, false, false, false, false, false)
        assert(response is RequestInterceptor.InterceptionResponse.AppIntent)
    }

    @Test
    fun `blocklisted schemes request always ignored even if the engine does not support it`() {
        val engineSession: EngineSession = mock()
        val supportedScheme = "supported"
        val notSupportedScheme = "not_supported"
        val feature = AppLinksInterceptor(
            context = mockContext,
            interceptLinkClicks = false,
            engineSupportedSchemes = setOf(supportedScheme),
            alwaysDeniedSchemes = setOf(notSupportedScheme),
            launchInApp = { false },
            useCases = mockUseCases,
        )

        val notSupportedUrl = "$notSupportedScheme://example.com"
        val notSupportedRedirect = AppLinkRedirect(Intent.parseUri(notSupportedUrl, 0), null, null)
        whenever(mockGetRedirect.invoke(notSupportedUrl)).thenReturn(notSupportedRedirect)
        val response = feature.onLoadRequest(engineSession, notSupportedUrl, null, false, false, false, false, false)
        assertEquals(null, response)
    }

    @Test
    fun `not supported schemes request should not use fallback if user preference is launch in app`() {
        val engineSession: EngineSession = mock()
        val supportedScheme = "supported"
        val notSupportedScheme = "not_supported"
        val blocklistedScheme = "blocklisted"
        val feature = AppLinksInterceptor(
            context = mockContext,
            interceptLinkClicks = false,
            engineSupportedSchemes = setOf(supportedScheme),
            alwaysDeniedSchemes = setOf(blocklistedScheme),
            launchInApp = { true },
            useCases = mockUseCases,
        )

        val notSupportedUrl = "$notSupportedScheme://example.com"
        val notSupportedRedirect = AppLinkRedirect(Intent.parseUri(notSupportedUrl, 0), fallbackUrl, null)
        whenever(mockGetRedirect.invoke(notSupportedUrl)).thenReturn(notSupportedRedirect)
        val response = feature.onLoadRequest(engineSession, notSupportedUrl, null, false, false, false, false, false)
        assert(response is RequestInterceptor.InterceptionResponse.AppIntent)
    }

    @Test
    fun `not supported schemes request uses fallback URL if available and launchInApp is set to false`() {
        val engineSession: EngineSession = mock()
        val supportedScheme = "supported"
        val notSupportedScheme = "not_supported"
        val blocklistedScheme = "blocklisted"
        val feature = AppLinksInterceptor(
            context = mockContext,
            interceptLinkClicks = true,
            engineSupportedSchemes = setOf(supportedScheme),
            alwaysDeniedSchemes = setOf(blocklistedScheme),
            launchInApp = { false },
            useCases = mockUseCases,
        )

        val notSupportedUrl = "$notSupportedScheme://example.com"
        val fallbackUrl = "https://example.com"
        val notSupportedRedirect = AppLinkRedirect(Intent.parseUri(notSupportedUrl, 0), fallbackUrl, null)
        whenever(mockGetRedirect.invoke(notSupportedUrl)).thenReturn(notSupportedRedirect)
        val response = feature.onLoadRequest(engineSession, notSupportedUrl, null, true, false, false, false, false)
        assert(response is RequestInterceptor.InterceptionResponse.Url)
    }

    @Test
    fun `not supported schemes request uses fallback URL not market intent if launchInApp is set to false`() {
        val engineSession: EngineSession = mock()
        val supportedScheme = "supported"
        val notSupportedScheme = "not_supported"
        val blocklistedScheme = "blocklisted"
        val feature = AppLinksInterceptor(
            context = mockContext,
            interceptLinkClicks = true,
            engineSupportedSchemes = setOf(supportedScheme),
            alwaysDeniedSchemes = setOf(blocklistedScheme),
            launchInApp = { false },
            useCases = mockUseCases,
        )

        val notSupportedUrl = "$notSupportedScheme://example.com"
        val fallbackUrl = "https://example.com"
        val notSupportedRedirect = AppLinkRedirect(null, fallbackUrl, Intent.parseUri(marketplaceUrl, 0))
        whenever(mockGetRedirect.invoke(notSupportedUrl)).thenReturn(notSupportedRedirect)
        val response = feature.onLoadRequest(engineSession, notSupportedUrl, null, true, false, false, false, false)
        assert(response is RequestInterceptor.InterceptionResponse.Url)
    }

    @Test
    fun `intent scheme launch intent if fallback URL is unavailable and launchInApp is set to false`() {
        val engineSession: EngineSession = mock()
        val feature = AppLinksInterceptor(
            context = mockContext,
            interceptLinkClicks = false,
            launchInApp = { false },
            useCases = mockUseCases,
        )

        val intentUrl = "intent://example.com"
        val intentRedirect = AppLinkRedirect(Intent.parseUri(intentUrl, 0), null, null)
        whenever(mockGetRedirect.invoke(intentUrl)).thenReturn(intentRedirect)
        val response = feature.onLoadRequest(engineSession, intentUrl, null, true, false, false, false, false)
        assert(response is RequestInterceptor.InterceptionResponse.AppIntent)
    }

    @Test
    fun `intent scheme uses fallback URL if available and launchInApp is set to false`() {
        val engineSession: EngineSession = mock()
        val feature = AppLinksInterceptor(
            context = mockContext,
            interceptLinkClicks = false,
            launchInApp = { false },
            useCases = mockUseCases,
        )

        val intentUrl = "intent://example.com"
        val fallbackUrl = "https://example.com"
        val intentRedirect = AppLinkRedirect(Intent.parseUri(intentUrl, 0), fallbackUrl, null)
        whenever(mockGetRedirect.invoke(intentUrl)).thenReturn(intentRedirect)
        val response = feature.onLoadRequest(engineSession, intentUrl, null, true, false, false, false, false)
        assert(response is RequestInterceptor.InterceptionResponse.Url)
    }

    @Test
    fun `request is not intercepted for URLs with javascript scheme`() {
        val javascriptUri = "javascript:;"

        val appRedirect = AppLinkRedirect(Intent.parseUri(javascriptUri, 0), null, null)
        whenever(mockGetRedirect.invoke(javascriptUri)).thenReturn(appRedirect)

        val response = appLinksInterceptor.onLoadRequest(mockEngineSession, javascriptUri, null, true, true, false, false, false)
        assertEquals(null, response)
    }

    @Test
    fun `Use the fallback URL when no non-browser app is installed`() {
        val response = appLinksInterceptor.onLoadRequest(mockEngineSession, fallbackUrl, null, true, false, false, false, false)
        assert(response is RequestInterceptor.InterceptionResponse.Url)
    }

    @Test
    fun `use the market intent if target app is not installed`() {
        val response = appLinksInterceptor.onLoadRequest(mockEngineSession, marketplaceUrl, null, true, false, false, false, false)
        assert(response is RequestInterceptor.InterceptionResponse.AppIntent)
    }

    @Test
    fun `external app is launched when launch in app is set to true and it is user triggered`() {
        appLinksInterceptor = AppLinksInterceptor(
            context = mockContext,
            interceptLinkClicks = true,
            launchInApp = { true },
            useCases = mockUseCases,
            launchFromInterceptor = true,
        )

        val response = appLinksInterceptor.onLoadRequest(mockEngineSession, webUrlWithAppLink, null, true, false, false, false, false)
        assert(response is RequestInterceptor.InterceptionResponse.AppIntent)
        verify(mockOpenRedirect).invoke(any(), anyBoolean(), any())
    }

    @Test
    fun `try to use fallback url if user preference is not to launch in third party app`() {
        appLinksInterceptor = AppLinksInterceptor(
            context = mockContext,
            interceptLinkClicks = true,
            launchInApp = { false },
            useCases = mockUseCases,
            launchFromInterceptor = true,
        )

        val testRedirect = AppLinkRedirect(Intent.parseUri(intentUrl, 0), fallbackUrl, null)
        val response = appLinksInterceptor.handleRedirect(testRedirect, intentUrl)
        assert(response is RequestInterceptor.InterceptionResponse.Url)
    }

    @Test
    fun `external app is launched when url scheme is not supported by the engine`() {
        appLinksInterceptor = AppLinksInterceptor(
            context = mockContext,
            interceptLinkClicks = true,
            launchInApp = { false },
            useCases = mockUseCases,
            launchFromInterceptor = true,
        )

        val response = appLinksInterceptor.onLoadRequest(mockEngineSession, intentUrl, null, false, true, false, false, false)
        assert(response is RequestInterceptor.InterceptionResponse.AppIntent)
        verify(mockOpenRedirect).invoke(any(), anyBoolean(), any())
    }

    @Test
    fun `do not use fallback url if trigger by user gesture and preference is to launch in app`() {
        appLinksInterceptor = AppLinksInterceptor(
            context = mockContext,
            interceptLinkClicks = true,
            launchInApp = { true },
            useCases = mockUseCases,
            launchFromInterceptor = true,
        )

        val testRedirect = AppLinkRedirect(Intent.parseUri(intentUrl, 0), fallbackUrl, null)
        val response = appLinksInterceptor.handleRedirect(testRedirect, intentUrl)
        assert(response is RequestInterceptor.InterceptionResponse.AppIntent)
    }

    @Test
    fun `launch marketplace intent if available and no external app`() {
        appLinksInterceptor = AppLinksInterceptor(
            context = mockContext,
            interceptLinkClicks = true,
            launchInApp = { true },
            useCases = mockUseCases,
            launchFromInterceptor = true,
        )

        val testRedirect = AppLinkRedirect(null, fallbackUrl, Intent.parseUri(marketplaceUrl, 0))
        val response = appLinksInterceptor.handleRedirect(testRedirect, webUrl)
        assert(response is RequestInterceptor.InterceptionResponse.AppIntent)
    }

    @Test
    fun `use fallback url if available and no external app`() {
        appLinksInterceptor = AppLinksInterceptor(
            context = mockContext,
            interceptLinkClicks = true,
            launchInApp = { true },
            useCases = mockUseCases,
            launchFromInterceptor = true,
        )

        val testRedirect = AppLinkRedirect(null, fallbackUrl, null)
        val response = appLinksInterceptor.handleRedirect(testRedirect, webUrl)
        assert(response is RequestInterceptor.InterceptionResponse.Url)
    }
}
