/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import android.app.DownloadManager.EXTRA_DOWNLOAD_ID
import android.content.Intent
import androidx.localbroadcastmanager.content.LocalBroadcastManager
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.session.Download
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.feature.downloads.ext.putDownloadExtra
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
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
        service = spy(object : AbstractFetchDownloadService(broadcastManager) {
            override val httpClient = client
        })
    }

    @Test
    fun `begins download when started`() = runBlocking {
        val download = Download("https://example.com/file.txt", "file.txt")
        val response = Response(
            "https://example.com/file.txt",
            200,
            MutableHeaders(),
            Response.Body(mock())
        )
        doReturn(response).`when`(client).fetch(Request("https://example.com/file.txt"))
        doNothing().`when`(service).useFileStream(eq(download), eq(response), eq("file.txt"), any())

        val downloadIntent = Intent("ACTION_DOWNLOAD").apply {
            putExtra(EXTRA_DOWNLOAD_ID, 1L)
            putDownloadExtra(download)
        }

        service.onStartCommand(downloadIntent, 0)

        verify(service).useFileStream(eq(download), eq(response), eq("file.txt"), any())
        verify(broadcastManager).sendBroadcast(any())
        Unit
    }
}
