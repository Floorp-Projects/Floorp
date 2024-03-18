/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa

import android.content.Context
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.request.RequestInterceptor
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class WebAppInterceptorTest {
    private lateinit var mockContext: Context
    private lateinit var mockEngineSession: EngineSession
    private lateinit var mockManifestStorage: ManifestStorage
    private lateinit var webAppInterceptor: WebAppInterceptor

    private val webUrl = "https://example.com"
    private val webUrlWithWebApp = "https://google.com/maps/"
    private val webUrlOutOfScope = "https://google.com/search/"

    @Before
    fun setup() {
        mockContext = mock()
        mockEngineSession = mock()
        mockManifestStorage = mock()

        webAppInterceptor = WebAppInterceptor(
            context = mockContext,
            manifestStorage = mockManifestStorage,
            launchFromInterceptor = true,
        )
    }

    @Test
    fun `request is intercepted when navigating to an installed web app`() {
        whenever(mockManifestStorage.getInstalledScope(webUrlWithWebApp)).thenReturn(webUrlWithWebApp)
        whenever(mockManifestStorage.getStartUrlForInstalledScope(webUrlWithWebApp)).thenReturn(webUrlWithWebApp)

        val response = webAppInterceptor.onLoadRequest(mockEngineSession, webUrlWithWebApp, null, true, false, false, false, false)

        assert(response is RequestInterceptor.InterceptionResponse.Deny)
    }

    @Test
    fun `request is not intercepted when url is out of scope`() {
        whenever(mockManifestStorage.getInstalledScope(webUrlOutOfScope)).thenReturn(null)
        whenever(mockManifestStorage.getStartUrlForInstalledScope(webUrlOutOfScope)).thenReturn(null)

        val response = webAppInterceptor.onLoadRequest(mockEngineSession, webUrlOutOfScope, null, true, false, false, false, false)

        assertNull(response)
    }

    @Test
    fun `request is not intercepted when url is not part of a web app`() {
        whenever(mockManifestStorage.getInstalledScope(webUrl)).thenReturn(null)
        whenever(mockManifestStorage.getStartUrlForInstalledScope(webUrl)).thenReturn(null)

        val response = webAppInterceptor.onLoadRequest(mockEngineSession, webUrl, null, true, false, false, false, false)

        assertNull(response)
    }

    @Test
    fun `request is intercepted with app intent if not launchFromInterceptor`() {
        webAppInterceptor = WebAppInterceptor(
            context = mockContext,
            manifestStorage = mockManifestStorage,
            launchFromInterceptor = false,
        )

        whenever(mockManifestStorage.getInstalledScope(webUrlWithWebApp)).thenReturn(webUrlWithWebApp)
        whenever(mockManifestStorage.getStartUrlForInstalledScope(webUrlWithWebApp)).thenReturn(webUrlWithWebApp)

        val response = webAppInterceptor.onLoadRequest(mockEngineSession, webUrlWithWebApp, null, true, false, false, false, false)

        assert(response is RequestInterceptor.InterceptionResponse.AppIntent)
    }

    @Test
    fun `launchFromInterceptor is enabled by default`() {
        webAppInterceptor = WebAppInterceptor(
            context = mockContext,
            manifestStorage = mockManifestStorage,
        )

        whenever(mockManifestStorage.getInstalledScope(webUrlWithWebApp)).thenReturn(webUrlWithWebApp)
        whenever(mockManifestStorage.getStartUrlForInstalledScope(webUrlWithWebApp)).thenReturn(webUrlWithWebApp)

        val response = webAppInterceptor.onLoadRequest(mockEngineSession, webUrlWithWebApp, null, true, false, false, false, false)

        assert(response is RequestInterceptor.InterceptionResponse.Deny)
    }
}
