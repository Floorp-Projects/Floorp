/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads.share

import android.content.Context
import android.webkit.MimeTypeMap
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.Dispatchers
import mozilla.components.browser.state.action.ShareInternetResourceAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.content.ShareInternetResourceState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Headers.Names.CONTENT_TYPE
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doAnswer
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.Shadows.shadowOf
import java.io.File
import java.nio.charset.StandardCharsets

/**
 * The 89a gif header as seen on https://www.w3.org/Graphics/GIF/spec-gif89a.txt
 */
private const val GIF_HEADER = "GIF89a"

@RunWith(AndroidJUnit4::class)
class ShareDownloadFeatureTest {
    // When writing new tests initialize ShareDownloadFeature with this class' context property
    // When creating new directories use class' context property#cacheDir as a parent
    // This will ensure the effectiveness of @After. Otherwise leftover files may be left on the machine running tests.

    private lateinit var context: Context
    private val testCacheDirName = "testCacheDir"

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val dispatcher = coroutinesTestRule.testDispatcher
    private val scope = coroutinesTestRule.scope

    @Before
    fun setup() {
        // Effectively reset context mock
        context = spy(testContext)
        doReturn(File(testCacheDirName)).`when`(context).cacheDir
    }

    @After
    fun cleanup() {
        context.cacheDir.deleteRecursively()
    }

    @Test
    fun `cleanupCache should automatically be called when this class is initialized`() = runTestOnMain {
        val cacheDir = File(context.cacheDir, cacheDirName).also { dir ->
            dir.mkdirs()
            File(dir, "leftoverFile").also { file ->
                file.createNewFile()
            }
        }

        assertTrue(cacheDir.listFiles()!!.isNotEmpty())

        ShareDownloadFeature(context, mock(), mock(), null, Dispatchers.Main)

        assertTrue(cacheDir.listFiles()!!.isEmpty())
    }

    @Test
    fun `ShareFeature starts the share process for AddShareAction which is immediately consumed`() {
        val store = spy(
            BrowserStore(
                BrowserState(
                    tabs = listOf(TabSessionState("123", ContentState(url = "https://www.mozilla.org"))),
                ),
            ),
        )
        val shareFeature = spy(ShareDownloadFeature(context, mock(), store, "123"))
        doNothing().`when`(shareFeature).startSharing(any())
        val download = ShareInternetResourceState(url = "testDownload")
        val action = ShareInternetResourceAction.AddShareAction("123", download)
        shareFeature.start()

        store.dispatch(action).joinBlocking()
        dispatcher.scheduler.advanceUntilIdle()

        verify(shareFeature).startSharing(download)
        verify(store).dispatch(ShareInternetResourceAction.ConsumeShareAction("123"))
    }

    @Test
    fun `cleanupCache should delete all files from the cache directory`() = runTestOnMain {
        val shareFeature = spy(ShareDownloadFeature(context, mock(), mock(), null, Dispatchers.Main))
        val testDir = File(context.cacheDir, cacheDirName).also { dir ->
            dir.mkdirs()
            File(dir, "testFile").also { file ->
                file.createNewFile()
            }
        }

        doReturn(testDir).`when`(shareFeature).getCacheDirectory()
        assertTrue(testDir.listFiles()!!.isNotEmpty())

        shareFeature.cleanupCache()

        assertTrue(testDir.listFiles()!!.isEmpty())
    }

    @Test
    fun `startSharing() will download and then share the selected download`() = runTestOnMain {
        val shareFeature = spy(ShareDownloadFeature(context, mock(), mock(), null))
        val shareState = ShareInternetResourceState(url = "testUrl", contentType = "contentType")
        val downloadedFile = File("filePath")
        doReturn(downloadedFile).`when`(shareFeature).download(any())
        shareFeature.scope = scope

        shareFeature.startSharing(shareState)

        verify(shareFeature).download(shareState)
        verify(shareFeature).share(downloadedFile.canonicalPath, "contentType", null, "testUrl")
        Unit
    }

    @Test
    fun `startSharing() will not use a too long HTTP url as message`() = runTestOnMain {
        val shareFeature = spy(ShareDownloadFeature(context, mock(), mock(), null))
        val maxSizeUrl = "a".repeat(CHARACTERS_IN_SHARE_TEXT_LIMIT)
        val tooLongUrl = maxSizeUrl + 'x'
        val shareState = ShareInternetResourceState(url = tooLongUrl, contentType = "contentType")
        val downloadedFile = File("filePath")
        doReturn(downloadedFile).`when`(shareFeature).download(any())
        shareFeature.scope = scope

        shareFeature.startSharing(shareState)

        verify(shareFeature).download(shareState)
        verify(shareFeature).share(downloadedFile.canonicalPath, "contentType", null, maxSizeUrl)
        Unit
    }

    @Test
    fun `startSharing() will use an empty String as message for data URLs`() = runTestOnMain {
        val shareFeature = spy(ShareDownloadFeature(context, mock(), mock(), null))
        val shareState = ShareInternetResourceState(url = "data:image/png;base64,longstring", contentType = "contentType")
        val downloadedFile = File("filePath")
        doReturn(downloadedFile).`when`(shareFeature).download(any())
        shareFeature.scope = scope

        shareFeature.startSharing(shareState)

        verify(shareFeature).download(shareState)
        verify(shareFeature).share(downloadedFile.canonicalPath, "contentType", null, "")
        Unit
    }

    @Test
    fun `download() will persist in cache the response#body() if available`() {
        val shareFeature = ShareDownloadFeature(context, mock(), mock(), null)
        val inputStream = "test".byteInputStream(StandardCharsets.UTF_8)
        val responseFromShareState = mock<Response>()
        doReturn(Response.Body(inputStream)).`when`(responseFromShareState).body
        val shareState = ShareInternetResourceState("randomUrl.jpg", response = responseFromShareState)
        doReturn(Response.SUCCESS).`when`(responseFromShareState).status
        doReturn(MutableHeaders()).`when`(responseFromShareState).headers

        val result = shareFeature.download(shareState)

        assertTrue(result.exists())
        assertTrue(result.name.endsWith(".$DEFAULT_IMAGE_EXTENSION"))
        assertEquals(cacheDirName, result.parentFile!!.name)
        assertEquals("test", result.inputStream().bufferedReader().use { it.readText() })
    }

    @Test(expected = RuntimeException::class)
    fun `download() will throw an error if the request is not successful`() {
        val shareFeature = ShareDownloadFeature(context, mock(), mock(), null)
        val inputStream = "test".byteInputStream(StandardCharsets.UTF_8)
        val responseFromShareState = mock<Response>()
        doReturn(Response.Body(inputStream)).`when`(responseFromShareState).body
        val shareState =
            ShareInternetResourceState("randomUrl.jpg", response = responseFromShareState)
        doReturn(500).`when`(responseFromShareState).status

        shareFeature.download(shareState)
    }

    @Test
    fun `download() will download from the provided url the response#body() if is unavailable`() {
        val client: Client = mock()
        val inputStream = "clientTest".byteInputStream(StandardCharsets.UTF_8)
        doAnswer { Response("randomUrl", 200, MutableHeaders(), Response.Body(inputStream)) }
            .`when`(client).fetch(any())
        val shareFeature = ShareDownloadFeature(context, client, mock(), null, Dispatchers.Main)
        val shareState = ShareInternetResourceState("randomUrl")

        val result = shareFeature.download(shareState)

        assertTrue(result.exists())
        assertTrue(result.name.endsWith(".$DEFAULT_IMAGE_EXTENSION"))
        assertEquals(mozilla.components.feature.downloads.share.cacheDirName, result.parentFile!!.name)
        assertEquals("clientTest", result.inputStream().bufferedReader().use { it.readText() })
    }

    @Test
    fun `download() will create a not private Request if not in private mode`() {
        val client: Client = mock()
        val requestCaptor = argumentCaptor<Request>()
        val inputStream = "clientTest".byteInputStream(StandardCharsets.UTF_8)
        doAnswer { Response("randomUrl.png", 200, MutableHeaders(), Response.Body(inputStream)) }
            .`when`(client).fetch(requestCaptor.capture())
        val shareFeature = ShareDownloadFeature(context, client, mock(), null)
        val shareState = ShareInternetResourceState("randomUrl.png", private = false)

        shareFeature.download(shareState)

        assertFalse(requestCaptor.value.private)
    }

    @Test
    fun `download() will create a private Request if in private mode`() {
        val client: Client = mock()
        val requestCaptor = argumentCaptor<Request>()
        val inputStream = "clientTest".byteInputStream(StandardCharsets.UTF_8)
        doAnswer { Response("randomUrl.png", 200, MutableHeaders(), Response.Body(inputStream)) }
            .`when`(client).fetch(requestCaptor.capture())
        val shareFeature = ShareDownloadFeature(context, client, mock(), null)
        val shareState = ShareInternetResourceState("randomUrl.png", private = true)

        shareFeature.download(shareState)

        assertTrue(requestCaptor.value.private)
    }

    @Test
    fun `getFilename(extension) will return a String with the extension suffix`() {
        val shareFeature = ShareDownloadFeature(context, mock(), mock(), null)
        val testExtension = "testExtension"

        val result = shareFeature.getFilename(testExtension)

        assertTrue(result.endsWith(testExtension))
        assertTrue(result.length > testExtension.length)
    }

    @Test
    fun `getTempFile(extension) will return a File from the cache dir and with name ending in extension`() {
        val shareFeature = spy(ShareDownloadFeature(context, mock(), mock(), null))
        val testExtension = "testExtension"

        val result = shareFeature.getTempFile(testExtension)

        assertTrue(result.name.endsWith(testExtension))
        assertEquals(shareFeature.getCacheDirectory().toString(), result.parent)
    }

    @Test
    fun `getCacheDirectory() will return a new directory in the app's cache`() {
        val shareFeature = ShareDownloadFeature(context, mock(), mock(), null)

        val result = shareFeature.getCacheDirectory()

        assertEquals(testCacheDirName, result.parent)
        assertEquals(cacheDirName, result.name)
    }

    @Test
    fun `getMediaShareCacheDirectory creates the needed files if they don't exist`() {
        val shareFeature = spy(ShareDownloadFeature(context, mock(), mock(), null))
        assertFalse(context.cacheDir.exists())

        val result = shareFeature.getMediaShareCacheDirectory()

        assertEquals(cacheDirName, result.name)
        assertTrue(result.exists())
    }

    @Test
    fun `getFileExtension returns a default extension if one cannot be extracted`() {
        val shareFeature = ShareDownloadFeature(context, mock(), mock(), null)

        val result = shareFeature.getFileExtension(mock(), mock())

        assertEquals(DEFAULT_IMAGE_EXTENSION, result)
    }

    @Test
    fun `getFileExtension returns an extension based on the media type inferred from the stream`() {
        val shareFeature = ShareDownloadFeature(context, mock(), mock(), null)
        val gifStream = (GIF_HEADER + "testImage").byteInputStream(StandardCharsets.UTF_8)
        // Add the gif mapping to a by default empty shadow of MimeTypeMap.
        shadowOf(MimeTypeMap.getSingleton()).addExtensionMimeTypMapping("gif", "image/gif")

        val result = shareFeature.getFileExtension(mock(), gifStream)

        assertEquals("gif", result)
    }

    @Test
    fun `getFileExtension returns an extension based on the response headers`() {
        val shareFeature = ShareDownloadFeature(context, mock(), mock(), null)
        val gifHeaders = MutableHeaders().apply {
            set(CONTENT_TYPE, "image/gif")
        }
        // Add the gif mapping to a by default empty shadow of MimeTypeMap.
        shadowOf(MimeTypeMap.getSingleton()).addExtensionMimeTypMapping("gif", "image/gif")

        val result = shareFeature.getFileExtension(gifHeaders, mock())

        assertEquals("gif", result)
    }

    @Test
    fun `sanitizeLongUrls should return an empty string for a data url`() {
        val shareFeature = ShareDownloadFeature(context, mock(), mock(), null)

        val result = shareFeature.sanitizeLongUrls("data:image/png;base64,longstring")

        assertEquals("", result)
    }

    @Test
    fun `sanitizeLongUrls should return at most CHARACTERS_IN_SHARE_TEXT_LIMIT for HTTP urls`() {
        val maxSizeUrl = "a".repeat(CHARACTERS_IN_SHARE_TEXT_LIMIT)
        val tooLongUrl = maxSizeUrl + 'x'
        val shareFeature = ShareDownloadFeature(context, mock(), mock(), null)

        val result = shareFeature.sanitizeLongUrls(tooLongUrl)

        assertEquals(CHARACTERS_IN_SHARE_TEXT_LIMIT, result.length)
        assertEquals(maxSizeUrl, result)
    }
}
