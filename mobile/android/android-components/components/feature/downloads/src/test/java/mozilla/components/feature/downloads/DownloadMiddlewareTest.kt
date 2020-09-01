/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import android.app.DownloadManager.EXTRA_DOWNLOAD_ID
import android.content.Context
import android.content.Intent
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.flow
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.runBlockingTest
import kotlinx.coroutines.test.setMain
import mozilla.components.browser.state.action.DownloadAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.state.content.DownloadState.Status.COMPLETED
import mozilla.components.browser.state.state.content.DownloadState.Status.INITIATED
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import org.mockito.Mockito.times
import org.mockito.Mockito.never

@RunWith(AndroidJUnit4::class)
class DownloadMiddlewareTest {

    private lateinit var dispatcher: TestCoroutineDispatcher
    private lateinit var scope: CoroutineScope

    @Before
    fun setUp() {
        dispatcher = TestCoroutineDispatcher()
        scope = CoroutineScope(dispatcher)

        Dispatchers.setMain(dispatcher)
    }

    @After
    fun tearDown() {
        dispatcher.cleanupTestCoroutines()
        scope.cancel()

        Dispatchers.resetMain()
    }

    @Test
    fun `service is started when download is queued`() = runBlockingTest {
        val applicationContext: Context = mock()
        val downloadMiddleware = DownloadMiddleware(
            applicationContext,
            AbstractFetchDownloadService::class.java,
            coroutineContext = dispatcher,
            downloadStorage = mock()
        )
        val store = BrowserStore(
            initialState = BrowserState(),
            middleware = listOf(downloadMiddleware)
        )

        val download = DownloadState("https://mozilla.org/download", destinationDirectory = "")
        store.dispatch(DownloadAction.AddDownloadAction(download)).joinBlocking()

        val intentCaptor = argumentCaptor<Intent>()
        verify(applicationContext).startService(intentCaptor.capture())
        assertEquals(download.id, intentCaptor.value.getStringExtra(EXTRA_DOWNLOAD_ID))
    }

    @Test
    fun `only restarted downloads MUST not be passed to the downloadStorage`() = runBlockingTest {
        val applicationContext: Context = mock()
        val downloadStorage: DownloadStorage = mock()
        val downloadMiddleware = DownloadMiddleware(
            applicationContext,
            AbstractFetchDownloadService::class.java,
            downloadStorage = downloadStorage,
            coroutineContext = dispatcher
        )
        val store = BrowserStore(
                initialState = BrowserState(),
                middleware = listOf(downloadMiddleware)
        )

        var download = DownloadState("https://mozilla.org/download", destinationDirectory = "")
        store.dispatch(DownloadAction.RestoreDownloadStateAction(download)).joinBlocking()

        verify(downloadStorage, never()).add(download)

        download = DownloadState("https://mozilla.org/download", destinationDirectory = "")
        store.dispatch(DownloadAction.AddDownloadAction(download)).joinBlocking()

        verify(downloadStorage).add(download)
    }

    @Test
    fun `RemoveDownloadAction MUST remove from the storage`() = runBlockingTest {
        val applicationContext: Context = mock()
        val downloadStorage: DownloadStorage = mock()
        val downloadMiddleware = DownloadMiddleware(
            applicationContext,
            AbstractFetchDownloadService::class.java,
            downloadStorage = downloadStorage,
            coroutineContext = dispatcher
        )
        val store = BrowserStore(
            initialState = BrowserState(),
            middleware = listOf(downloadMiddleware)
        )

        val download = DownloadState("https://mozilla.org/download", destinationDirectory = "")
        store.dispatch(DownloadAction.AddDownloadAction(download)).joinBlocking()

        store.dispatch(DownloadAction.RemoveDownloadAction(download.id)).joinBlocking()

        verify(downloadStorage).remove(download)
    }

    @Test
    fun `RemoveAllDownloadsAction MUST remove all downloads from the storage`() = runBlockingTest {
        val applicationContext: Context = mock()
        val downloadStorage: DownloadStorage = mock()
        val downloadMiddleware = DownloadMiddleware(
            applicationContext,
            AbstractFetchDownloadService::class.java,
            downloadStorage = downloadStorage,
            coroutineContext = dispatcher
        )
        val store = BrowserStore(
            initialState = BrowserState(),
            middleware = listOf(downloadMiddleware)
        )

        val download = DownloadState("https://mozilla.org/download", destinationDirectory = "")
        store.dispatch(DownloadAction.AddDownloadAction(download)).joinBlocking()

        store.dispatch(DownloadAction.RemoveAllDownloadsAction).joinBlocking()

        verify(downloadStorage).removeAllDownloads()
    }

    @Test
    fun `UpdateDownloadAction MUST update the storage when changes are needed`() = runBlockingTest {
        val applicationContext: Context = mock()
        val downloadStorage: DownloadStorage = mock()
        val downloadMiddleware = DownloadMiddleware(
            applicationContext,
            AbstractFetchDownloadService::class.java,
            downloadStorage = downloadStorage,
            coroutineContext = dispatcher
        )
        val store = BrowserStore(
            initialState = BrowserState(),
            middleware = listOf(downloadMiddleware)
        )

        val download = DownloadState("https://mozilla.org/download", status = INITIATED)
        store.dispatch(DownloadAction.AddDownloadAction(download)).joinBlocking()

        val downloadInTheStore = store.state.downloads.getValue(download.id)

        assertEquals(download, downloadInTheStore)

        var updatedDownload = download.copy(status = COMPLETED, skipConfirmation = true)
        store.dispatch(DownloadAction.UpdateDownloadAction(updatedDownload)).joinBlocking()

        verify(downloadStorage).update(updatedDownload)

        // skipConfirmation is value that we are not storing in the storage,
        // changes on it shouldn't trigger an update on the storage.
        updatedDownload = updatedDownload.copy(skipConfirmation = false)
        store.dispatch(DownloadAction.UpdateDownloadAction(updatedDownload)).joinBlocking()

        verify(downloadStorage, times(1)).update(any())
    }

    @Test
    fun `RestoreDownloadsState MUST populate the store with items in the storage`() = runBlockingTest {
        val applicationContext: Context = mock()
        val downloadStorage: DownloadStorage = mock()
        val dispatcher = TestCoroutineDispatcher()
        val downloadMiddleware = DownloadMiddleware(
            applicationContext,
            AbstractFetchDownloadService::class.java,
            downloadStorage = downloadStorage,
            coroutineContext = dispatcher
        )
        val store = BrowserStore(
            initialState = BrowserState(),
            middleware = listOf(downloadMiddleware)
        )

        val download = DownloadState("https://mozilla.org/download")
        whenever(downloadStorage.getDownloads()).thenReturn(
            flow {
                emit(listOf(download))
            }
        )

        assertTrue(store.state.downloads.isEmpty())

        store.dispatch(DownloadAction.RestoreDownloadsStateAction).joinBlocking()

        dispatcher.advanceUntilIdle()
        store.waitUntilIdle()

        assertEquals(download, store.state.downloads.values.first())
    }
}
