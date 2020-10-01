/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.DownloadAction
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.mock
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class DownloadUseCasesTest {

    @Test
    fun consumeDownloadUseCase() {
        val store: BrowserStore = mock()
        val useCases = DownloadsUseCases(store)

        useCases.consumeDownload("tabId", "downloadId")
        verify(store).dispatch(ContentAction.ConsumeDownloadAction("tabId", "downloadId"))
    }

    @Test
    fun restoreDownloadsUseCase() {
        val store: BrowserStore = mock()
        val useCases = DownloadsUseCases(store)

        useCases.restoreDownloads()
        verify(store).dispatch(DownloadAction.RestoreDownloadsStateAction)
    }

    @Test
    fun removeDownloadUseCase() {
        val store: BrowserStore = mock()
        val useCases = DownloadsUseCases(store)

        useCases.removeDownload("downloadId")
        verify(store).dispatch(DownloadAction.RemoveDownloadAction("downloadId"))
    }

    @Test
    fun removeAllDownloadsUseCase() {
        val store: BrowserStore = mock()
        val useCases = DownloadsUseCases(store)

        useCases.removeAllDownloads()
        verify(store).dispatch(DownloadAction.RemoveAllDownloadsAction)
    }
}
