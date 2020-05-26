/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import android.app.DownloadManager.EXTRA_DOWNLOAD_ID
import android.content.Context
import android.content.Intent
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.action.DownloadAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class DownloadMiddlewareTest {

    @Test
    fun `service is started when download is queued`() {
        val applicationContext: Context = mock()
        val store = BrowserStore(
            initialState = BrowserState(),
            middleware = listOf(DownloadMiddleware(applicationContext, AbstractFetchDownloadService::class.java))
        )

        val download = DownloadState("https://mozilla.org/download", destinationDirectory = "")
        store.dispatch(DownloadAction.QueueDownloadAction(download)).joinBlocking()

        val intentCaptor = argumentCaptor<Intent>()
        verify(applicationContext).startService(intentCaptor.capture())
        assertEquals(download.id, intentCaptor.value.getLongExtra(EXTRA_DOWNLOAD_ID, -1))
    }
}
