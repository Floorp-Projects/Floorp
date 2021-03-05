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
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.runBlockingTest
import kotlinx.coroutines.test.setMain
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.DownloadAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.state.content.DownloadState.Status.COMPLETED
import mozilla.components.browser.state.state.content.DownloadState.Status.INITIATED
import mozilla.components.browser.state.state.content.DownloadState.Status.FAILED
import mozilla.components.browser.state.state.content.DownloadState.Status.CANCELLED
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.fetch.Response
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
import org.mockito.Mockito.spy
import org.mockito.Mockito.reset

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
        val downloadMiddleware = spy(DownloadMiddleware(
            applicationContext,
            AbstractFetchDownloadService::class.java,
            coroutineContext = dispatcher,
            downloadStorage = mock()
        ))
        val store = BrowserStore(
            initialState = BrowserState(),
            middleware = listOf(downloadMiddleware)
        )

        val download = DownloadState("https://mozilla.org/download", destinationDirectory = "")
        store.dispatch(DownloadAction.AddDownloadAction(download)).joinBlocking()

        val intentCaptor = argumentCaptor<Intent>()
        verify(downloadMiddleware).startForegroundService(intentCaptor.capture())
        assertEquals(download.id, intentCaptor.value.getStringExtra(EXTRA_DOWNLOAD_ID))

        reset(downloadMiddleware)

        // We don't store private downloads in the storage.
        val privateDownload = download.copy(id = "newId", private = true)

        store.dispatch(DownloadAction.AddDownloadAction(privateDownload)).joinBlocking()

        verify(downloadMiddleware, never()).saveDownload(any(), any())
        verify(downloadMiddleware.downloadStorage, never()).add(privateDownload)
        verify(downloadMiddleware).startForegroundService(intentCaptor.capture())
        assertEquals(privateDownload.id, intentCaptor.value.getStringExtra(EXTRA_DOWNLOAD_ID))
    }

    @Test
    fun `saveDownload do not store private downloads`() = runBlockingTest {
        val applicationContext: Context = mock()
        val downloadMiddleware = spy(DownloadMiddleware(
            applicationContext,
            AbstractFetchDownloadService::class.java,
            coroutineContext = dispatcher,
            downloadStorage = mock()
        ))
        val store = BrowserStore(
            initialState = BrowserState(),
            middleware = listOf(downloadMiddleware)
        )

        val privateDownload = DownloadState("https://mozilla.org/download", private = true)

        store.dispatch(DownloadAction.AddDownloadAction(privateDownload)).joinBlocking()

        verify(downloadMiddleware.downloadStorage, never()).add(privateDownload)
    }

    @Test
    fun `restarted downloads MUST not be passed to the downloadStorage`() = runBlockingTest {
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
    fun `previously added downloads MUST be ignored`() = runBlockingTest {
        val applicationContext: Context = mock()
        val downloadStorage: DownloadStorage = mock()
        val download = DownloadState("https://mozilla.org/download")
        val downloadMiddleware = DownloadMiddleware(
            applicationContext,
            AbstractFetchDownloadService::class.java,
            downloadStorage = downloadStorage,
            coroutineContext = dispatcher
        )
        val store = BrowserStore(
            initialState = BrowserState(
                downloads = mapOf(download.id to download)
            ),
            middleware = listOf(downloadMiddleware)
        )

        store.dispatch(DownloadAction.AddDownloadAction(download)).joinBlocking()

        verify(downloadStorage, never()).add(download)
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

        // Private downloads are not updated in the storage.
        updatedDownload = updatedDownload.copy(private = true)

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
        whenever(downloadStorage.getDownloadsList()).thenReturn(listOf(download))

        assertTrue(store.state.downloads.isEmpty())

        store.dispatch(DownloadAction.RestoreDownloadsStateAction).joinBlocking()

        dispatcher.advanceUntilIdle()
        store.waitUntilIdle()

        assertEquals(download, store.state.downloads.values.first())
    }

    @Test
    fun `private downloads MUST NOT be restored`() = runBlockingTest {
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

        val download = DownloadState("https://mozilla.org/download", private = true)
        whenever(downloadStorage.getDownloadsList()).thenReturn(listOf(download))

        assertTrue(store.state.downloads.isEmpty())

        store.dispatch(DownloadAction.RestoreDownloadsStateAction).joinBlocking()

        dispatcher.advanceUntilIdle()
        store.waitUntilIdle()

        assertTrue(store.state.downloads.isEmpty())
    }

    @Test
    fun `sendDownloadIntent MUST call startForegroundService WHEN downloads are NOT COMPLETED, CANCELLED and FAILED`() = runBlockingTest {
        val applicationContext: Context = mock()
        val downloadMiddleware = spy(DownloadMiddleware(
            applicationContext,
            AbstractFetchDownloadService::class.java
        ))

        val ignoredStatus = listOf(COMPLETED, CANCELLED, FAILED)
        ignoredStatus.forEach { status ->
            val download = DownloadState("https://mozilla.org/download", status = status)
            downloadMiddleware.sendDownloadIntent(download)
            verify(downloadMiddleware, times(0)).startForegroundService(any())
        }

        reset(downloadMiddleware)

        val allowedStatus = DownloadState.Status.values().filter { it !in ignoredStatus }

        allowedStatus.forEachIndexed { index, status ->
            val download = DownloadState("https://mozilla.org/download", status = status)
            downloadMiddleware.sendDownloadIntent(download)
            verify(downloadMiddleware, times(index + 1)).startForegroundService(any())
        }
    }

    @Test
    fun `WHEN RemoveAllTabsAction and RemoveAllPrivateTabsAction are received THEN removePrivateNotifications must be called`() = runBlockingTest {
        val applicationContext: Context = mock()
        val downloadMiddleware = spy(DownloadMiddleware(
            applicationContext,
            AbstractFetchDownloadService::class.java,
            coroutineContext = dispatcher,
            downloadStorage = mock()
        ))
        val store = BrowserStore(
            initialState = BrowserState(),
            middleware = listOf(downloadMiddleware)
        )

        val actions = listOf(TabListAction.RemoveAllTabsAction, TabListAction.RemoveAllPrivateTabsAction)

        actions.forEach {
            store.dispatch(it).joinBlocking()

            dispatcher.advanceUntilIdle()
            store.waitUntilIdle()

            verify(downloadMiddleware, times(1)).removePrivateNotifications(any())
            reset(downloadMiddleware)
        }
    }

    @Test
    fun `WHEN RemoveTabsAction is received THEN removePrivateNotifications must be called`() = runBlockingTest {
        val applicationContext: Context = mock()
        val downloadMiddleware = spy(DownloadMiddleware(
            applicationContext,
            AbstractFetchDownloadService::class.java,
            coroutineContext = dispatcher,
            downloadStorage = mock()
        ))
        val store = BrowserStore(
            initialState = BrowserState(),
            middleware = listOf(downloadMiddleware)
        )

        store.dispatch(TabListAction.RemoveTabsAction(listOf("tab1"))).joinBlocking()

        dispatcher.advanceUntilIdle()
        store.waitUntilIdle()

        verify(downloadMiddleware, times(1)).removePrivateNotifications(any(), any())
        reset(downloadMiddleware)
    }

    @Test
    fun `WHEN RemoveTabAction is received THEN removePrivateNotifications must be called`() = runBlockingTest {
        val applicationContext: Context = mock()
        val downloadMiddleware = spy(DownloadMiddleware(
            applicationContext,
            AbstractFetchDownloadService::class.java,
            coroutineContext = dispatcher,
            downloadStorage = mock()
        ))
        val store = BrowserStore(
            initialState = BrowserState(),
            middleware = listOf(downloadMiddleware)
        )

        store.dispatch(TabListAction.RemoveTabAction("tab1")).joinBlocking()

        dispatcher.advanceUntilIdle()
        store.waitUntilIdle()

        verify(downloadMiddleware, times(1)).removePrivateNotifications(any(), any())
    }

    @Test
    fun `WHEN removeStatusBarNotification is called THEN an ACTION_REMOVE_PRIVATE_DOWNLOAD intent must be created`() = runBlockingTest {
        val applicationContext: Context = mock()
        val downloadMiddleware = spy(DownloadMiddleware(
            applicationContext,
            AbstractFetchDownloadService::class.java,
            coroutineContext = dispatcher,
            downloadStorage = mock()
        ))
        val download = DownloadState("https://mozilla.org/download", notificationId = 100)
        val store = mock<BrowserStore>()

        downloadMiddleware.removeStatusBarNotification(store, download)

        verify(store, times(1)).dispatch(DownloadAction.DismissDownloadNotificationAction(download.id))
        verify(applicationContext, times(1)).startService(any())
    }

    @Test
    fun `WHEN removePrivateNotifications is called THEN removeStatusBarNotification will be called only for private download`() = runBlockingTest {
        val applicationContext: Context = mock()
        val downloadMiddleware = spy(DownloadMiddleware(
            applicationContext,
            AbstractFetchDownloadService::class.java,
            coroutineContext = dispatcher,
            downloadStorage = mock()
        ))
        val download = DownloadState("https://mozilla.org/download", notificationId = 100)
        val privateDownload = DownloadState("https://mozilla.org/download", notificationId = 100, private = true)
        val store = BrowserStore(
            initialState = BrowserState(
                downloads = mapOf(download.id to download, privateDownload.id to privateDownload)
            ),
            middleware = listOf(downloadMiddleware)
        )

        downloadMiddleware.removePrivateNotifications(store)

        verify(downloadMiddleware, times(1)).removeStatusBarNotification(store, privateDownload)
    }

    @Test
    fun `WHEN removePrivateNotifications is called THEN removeStatusBarNotification will be called only for private download in the tabIds list`() = runBlockingTest {
        val applicationContext: Context = mock()
        val downloadMiddleware = spy(DownloadMiddleware(
            applicationContext,
            AbstractFetchDownloadService::class.java,
            coroutineContext = dispatcher,
            downloadStorage = mock()
        ))
        val download = DownloadState("https://mozilla.org/download", notificationId = 100, sessionId = "tab1")
        val privateDownload = DownloadState("https://mozilla.org/download", notificationId = 100, private = true, sessionId = "tab2")
        val anotherPrivateDownload = DownloadState("https://mozilla.org/download", notificationId = 100, private = true, sessionId = "tab3")
        val store = BrowserStore(
            initialState = BrowserState(
                downloads = mapOf(download.id to download, privateDownload.id to privateDownload, anotherPrivateDownload.id to anotherPrivateDownload)
            ),
            middleware = listOf(downloadMiddleware)
        )

        downloadMiddleware.removePrivateNotifications(store, listOf("tab2"))

        verify(downloadMiddleware, times(1)).removeStatusBarNotification(any(), any())
    }

    @Test
    fun `WHEN an action for canceling a download response is received THEN a download response must be canceled`() = runBlockingTest {
        val applicationContext: Context = mock()
        val downloadMiddleware = spy(DownloadMiddleware(
            applicationContext,
            AbstractFetchDownloadService::class.java,
            coroutineContext = dispatcher,
            downloadStorage = mock()
        ))
        val store = BrowserStore(
            initialState = BrowserState(),
            middleware = listOf(downloadMiddleware)
        )

        store.dispatch(ContentAction.CancelDownloadAction("tabID", "downloadID")).joinBlocking()

        dispatcher.advanceUntilIdle()
        store.waitUntilIdle()

        verify(downloadMiddleware, times(1)).closeDownloadResponse(any(), any())
    }

    @Test
    fun `WHEN closing a download response THEN the response object must be closed`() {
        val applicationContext: Context = mock()
        val downloadMiddleware = spy(DownloadMiddleware(
            applicationContext,
            AbstractFetchDownloadService::class.java,
            coroutineContext = dispatcher,
            downloadStorage = mock()
        ))
        val store = BrowserStore(
            initialState = BrowserState(),
            middleware = listOf(downloadMiddleware)
        )

        val tab = createTab("https://www.mozilla.org")
        val response = mock<Response>()
        val download = DownloadState(url = "https://www.mozilla.org/file.txt", sessionId = tab.id, response = response)

        store.dispatch(TabListAction.AddTabAction(tab, select = true)).joinBlocking()
        store.dispatch(ContentAction.UpdateDownloadAction(tab.id, download = download)).joinBlocking()

        downloadMiddleware.closeDownloadResponse(store, tab.id)
        verify(response).close()
    }
}
