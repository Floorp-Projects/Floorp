/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.icons.generator.IconGenerator
import mozilla.components.lib.fetch.httpurlconnection.HttpURLConnectionClient
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import okio.Okio
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class BrowserIconsTest {
    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    @Test
    fun `Uses generator`() {
        val mockedIcon: Icon = mock()

        val generator: IconGenerator = mock()
        `when`(generator.generate(any(), any())).thenReturn(mockedIcon)

        val request = IconRequest(url = "https://www.mozilla.org")
        val icon = BrowserIcons(context, httpClient = mock(), generator = generator)
            .loadIcon(request)

        assertEquals(mockedIcon, runBlocking { icon.await() })

        verify(generator).generate(context, request)
    }

    @Test
    fun `WHEN resources are provided THEN an icon will be downloaded from one of them`() = runBlocking {
        val server = MockWebServer()

        server.enqueue(MockResponse().setBody(
            Okio.buffer(Okio.source(javaClass.getResourceAsStream("/png/mozac.png")!!)).buffer()
        ))

        server.start()

        try {
            val request = IconRequest(
                url = server.url("/").toString(),
                size = IconRequest.Size.DEFAULT,
                resources = listOf(
                    IconRequest.Resource(
                        url = server.url("icon64.png").toString(),
                        sizes = listOf(IconRequest.Resource.Size(64, 64)),
                        mimeType = "image/png",
                        type = IconRequest.Resource.Type.FAVICON
                    ),
                    IconRequest.Resource(
                        url = server.url("icon128.png").toString(),
                        sizes = listOf(IconRequest.Resource.Size(128, 128)),
                        mimeType = "image/png",
                        type = IconRequest.Resource.Type.FAVICON
                    ),
                    IconRequest.Resource(
                        url = server.url("icon128.png").toString(),
                        sizes = listOf(IconRequest.Resource.Size(180, 180)),
                        type = IconRequest.Resource.Type.APPLE_TOUCH_ICON
                    )
                )
            )

            val icon = BrowserIcons(
                context,
                httpClient = HttpURLConnectionClient()
            ).loadIcon(request).await()

            assertNotNull(icon)
            assertNotNull(icon.bitmap!!)

            val serverRequest = server.takeRequest()
            assertEquals("/icon64.png", serverRequest.requestUrl.encodedPath())
        } finally {
            server.shutdown()
        }
    }
}
