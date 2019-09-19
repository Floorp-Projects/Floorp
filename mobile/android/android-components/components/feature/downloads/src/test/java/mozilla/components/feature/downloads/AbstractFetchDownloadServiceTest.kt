/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import android.app.DownloadManager.EXTRA_DOWNLOAD_ID
import android.content.Intent
import androidx.localbroadcastmanager.content.LocalBroadcastManager
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.feature.downloads.ext.putDownloadExtra
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.MockitoAnnotations.initMocks

@RunWith(AndroidJUnit4::class)
class AbstractFetchDownloadServiceTest {

    @Mock private lateinit var client: Client
    @Mock private lateinit var broadcastManager: LocalBroadcastManager
    private lateinit var service: AbstractFetchDownloadService

    @Before
    fun setup() {
        initMocks(this)
        service = spy(object : AbstractFetchDownloadService() {
            override val httpClient = client
        })
        doReturn(broadcastManager).`when`(service).broadcastManager
        doReturn(testContext).`when`(service).context
    }

    @Test
    fun `begins download when started`() = runBlocking {
        val download = DownloadState("https://example.com/file.txt", "file.txt")
        val response = Response(
            "https://example.com/file.txt",
            200,
            MutableHeaders(),
            Response.Body(mock())
        )
        doReturn(response).`when`(client).fetch(Request("https://example.com/file.txt"))
        doNothing().`when`(service).useFileStream(any(), any())

        val downloadIntent = Intent("ACTION_DOWNLOAD").apply {
            putExtra(EXTRA_DOWNLOAD_ID, 1L)
            putDownloadExtra(download)
        }

        service.onStartCommand(downloadIntent, 0)

        val providedDownload = argumentCaptor<DownloadState>()
        verify(service).useFileStream(providedDownload.capture(), any())
        assertEquals(download.url, providedDownload.value.url)
        assertEquals(download.fileName, providedDownload.value.fileName)

        verify(broadcastManager).sendBroadcast(any())
        Unit
    }
}
