/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons

import android.graphics.Bitmap
import android.graphics.drawable.Drawable
import android.widget.ImageView
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.Job
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.icons.generator.IconGenerator
import mozilla.components.concept.engine.manifest.Size
import mozilla.components.lib.fetch.httpurlconnection.HttpURLConnectionClient
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import okio.Okio
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertSame
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class BrowserIconsTest {

    @Before
    @After
    fun cleanUp() {
        sharedDiskCache.clear(testContext)
        sharedMemoryCache.clear()
    }

    @Test
    fun `Uses generator`() {
        val mockedIcon: Icon = mock()

        val generator: IconGenerator = mock()
        `when`(generator.generate(any(), any())).thenReturn(mockedIcon)

        val request = IconRequest(url = "https://www.mozilla.org")
        val icon = BrowserIcons(testContext, httpClient = mock(), generator = generator)
            .loadIcon(request)

        assertEquals(mockedIcon, runBlocking { icon.await() })

        verify(generator).generate(testContext, request)
    }

    @Test
    fun `WHEN resources are provided THEN an icon will be downloaded from one of them`() = runBlocking {
        val server = MockWebServer()

        server.enqueue(MockResponse().setBody(

            Okio.buffer(Okio.source(javaClass.getResourceAsStream("/png/mozac.png")!!)).buffer
        ))

        server.start()

        try {
            val request = IconRequest(
                url = server.url("/").toString(),
                size = IconRequest.Size.DEFAULT,
                resources = listOf(
                    IconRequest.Resource(
                        url = server.url("icon64.png").toString(),
                        sizes = listOf(Size(64, 64)),
                        mimeType = "image/png",
                        type = IconRequest.Resource.Type.FAVICON
                    ),
                    IconRequest.Resource(
                        url = server.url("icon128.png").toString(),
                        sizes = listOf(Size(128, 128)),
                        mimeType = "image/png",
                        type = IconRequest.Resource.Type.FAVICON
                    ),
                    IconRequest.Resource(
                        url = server.url("icon128.png").toString(),
                        sizes = listOf(Size(180, 180)),
                        type = IconRequest.Resource.Type.APPLE_TOUCH_ICON
                    )
                )
            )

            val icon = BrowserIcons(
                testContext,
                httpClient = HttpURLConnectionClient()
            ).loadIcon(request).await()

            assertNotNull(icon)
            assertNotNull(icon.bitmap)

            val serverRequest = server.takeRequest()
            assertEquals("/icon128.png", serverRequest.requestUrl.encodedPath())
        } finally {
            server.shutdown()
        }
    }

    @Test
    fun `WHEN icon is loaded twice THEN second load is delivered from memory cache`() = runBlocking {
        val server = MockWebServer()

        server.enqueue(MockResponse().setBody(
            Okio.buffer(Okio.source(javaClass.getResourceAsStream("/png/mozac.png")!!)).buffer
        ))

        server.start()

        try {
            val icons = BrowserIcons(testContext, httpClient = HttpURLConnectionClient())

            val request = IconRequest(
                url = "https://www.mozilla.org",
                resources = listOf(
                    IconRequest.Resource(
                        url = server.url("icon64.png").toString(),
                        type = IconRequest.Resource.Type.FAVICON
                    )
                )
            )

            val icon = icons.loadIcon(request).await()

            assertEquals(Icon.Source.DOWNLOAD, icon.source)
            assertNotNull(icon.bitmap)

            val secondIcon = icons.loadIcon(
                IconRequest("https://www.mozilla.org") // Without resources!
            ).await()

            assertEquals(Icon.Source.MEMORY, secondIcon.source)
            assertNotNull(secondIcon.bitmap)

            assertSame(icon.bitmap, secondIcon.bitmap)
        } finally {
            server.shutdown()
        }
    }

    @Test
    fun `WHEN icon is loaded again and not in memory cache THEN second load is delivered from disk cache`() = runBlocking {
        val server = MockWebServer()

        server.enqueue(MockResponse().setBody(
            Okio.buffer(Okio.source(javaClass.getResourceAsStream("/png/mozac.png")!!)).buffer
        ))

        server.start()

        try {
            val icons = BrowserIcons(testContext, httpClient = HttpURLConnectionClient())

            val request = IconRequest(
                url = "https://www.mozilla.org",
                resources = listOf(
                    IconRequest.Resource(
                        url = server.url("icon64.png").toString(),
                        type = IconRequest.Resource.Type.FAVICON
                    )
                )
            )

            val icon = icons.loadIcon(request).await()

            assertEquals(Icon.Source.DOWNLOAD, icon.source)
            assertNotNull(icon.bitmap)

            sharedMemoryCache.clear()

            val secondIcon = icons.loadIcon(
                IconRequest("https://www.mozilla.org") // Without resources!
            ).await()

            assertEquals(Icon.Source.DISK, secondIcon.source)
            assertNotNull(secondIcon.bitmap)
        } finally {
            server.shutdown()
        }
    }

    @Test
    fun `Automatically load icon into image view`() {
        val mockedBitmap: Bitmap = mock()
        val mockedIcon: Icon = mock()
        val result = CompletableDeferred<Icon>()
        val view: ImageView = mock()
        val icons = spy(BrowserIcons(testContext, httpClient = mock()))

        val request = IconRequest(url = "https://www.mozilla.org")

        doReturn(mockedBitmap).`when`(mockedIcon).bitmap
        doReturn(result).`when`(icons).loadIcon(request)

        val job = icons.loadIntoView(view, request)

        verify(view).setImageDrawable(null)
        verify(view).addOnAttachStateChangeListener(any())
        verify(view).setTag(eq(R.id.mozac_browser_icons_tag_job), any())
        verify(view, never()).setImageBitmap(any())

        result.complete(mockedIcon)
        job.joinBlocking()

        verify(view).setImageBitmap(mockedBitmap)
        verify(view).removeOnAttachStateChangeListener(any())
        verify(view).setTag(R.id.mozac_browser_icons_tag_job, null)
    }

    @Test
    fun `loadIntoView sets drawable to error if cancelled`() {
        val result = CompletableDeferred<Icon>()
        val view: ImageView = mock()
        val placeholder: Drawable = mock()
        val error: Drawable = mock()
        val icons = spy(BrowserIcons(testContext, httpClient = mock()))

        val request = IconRequest(url = "https://www.mozilla.org")

        doReturn(result).`when`(icons).loadIcon(request)

        val job = icons.loadIntoView(view, request, placeholder = placeholder, error = error)

        verify(view).setImageDrawable(placeholder)

        result.cancel()
        job.joinBlocking()

        verify(view).setImageDrawable(error)
        verify(view).removeOnAttachStateChangeListener(any())
        verify(view).setTag(R.id.mozac_browser_icons_tag_job, null)
    }

    @Test
    fun `loadIntoView cancels previous jobs`() {
        val result = CompletableDeferred<Icon>()
        val view: ImageView = mock()
        val previousJob: Job = mock()
        val icons = spy(BrowserIcons(testContext, httpClient = mock()))

        val request = IconRequest(url = "https://www.mozilla.org")

        doReturn(previousJob).`when`(view).getTag(R.id.mozac_browser_icons_tag_job)
        doReturn(result).`when`(icons).loadIcon(request)

        icons.loadIntoView(view, request)

        verify(previousJob).cancel()

        result.cancel()
    }
}
