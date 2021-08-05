/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import android.app.DownloadManager
import android.app.DownloadManager.EXTRA_DOWNLOAD_ID
import android.app.Notification
import android.app.NotificationManager
import android.app.Service
import android.content.ContentResolver
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Environment
import androidx.core.app.NotificationManagerCompat
import androidx.core.content.FileProvider
import androidx.core.content.getSystemService
import androidx.core.net.toUri
import androidx.localbroadcastmanager.content.LocalBroadcastManager
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.runBlockingTest
import kotlinx.coroutines.test.setMain
import mozilla.components.browser.state.action.DownloadAction
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.state.content.DownloadState.Status.COMPLETED
import mozilla.components.browser.state.state.content.DownloadState.Status.DOWNLOADING
import mozilla.components.browser.state.state.content.DownloadState.Status.FAILED
import mozilla.components.browser.state.state.content.DownloadState.Status.INITIATED
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.feature.downloads.AbstractFetchDownloadService.Companion.ACTION_CANCEL
import mozilla.components.feature.downloads.AbstractFetchDownloadService.Companion.ACTION_PAUSE
import mozilla.components.feature.downloads.AbstractFetchDownloadService.Companion.ACTION_REMOVE_PRIVATE_DOWNLOAD
import mozilla.components.feature.downloads.AbstractFetchDownloadService.Companion.ACTION_RESUME
import mozilla.components.feature.downloads.AbstractFetchDownloadService.Companion.ACTION_TRY_AGAIN
import mozilla.components.feature.downloads.AbstractFetchDownloadService.Companion.PROGRESS_UPDATE_INTERVAL
import mozilla.components.feature.downloads.AbstractFetchDownloadService.CopyInChuckStatus.ERROR_IN_STREAM_CLOSED
import mozilla.components.feature.downloads.AbstractFetchDownloadService.DownloadJobState
import mozilla.components.feature.downloads.DownloadNotification.NOTIFICATION_DOWNLOAD_GROUP_ID
import mozilla.components.feature.downloads.facts.DownloadsFacts.Items.NOTIFICATION
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.processor.CollectionProcessor
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.eq
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.ArgumentMatchers.anyLong
import org.mockito.ArgumentMatchers.anyString
import org.mockito.ArgumentMatchers.isNull
import org.mockito.Mock
import org.mockito.Mockito.atLeastOnce
import org.mockito.Mockito.doAnswer
import org.mockito.Mockito.doCallRealMethod
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.doThrow
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoInteractions
import org.mockito.MockitoAnnotations.openMocks
import org.robolectric.Shadows.shadowOf
import org.robolectric.annotation.Config
import org.robolectric.annotation.Implementation
import org.robolectric.annotation.Implements
import org.robolectric.shadows.ShadowNotificationManager
import java.io.File
import java.io.IOException
import java.io.InputStream
import java.lang.IllegalArgumentException
import kotlin.random.Random

@RunWith(AndroidJUnit4::class)
class AbstractFetchDownloadServiceTest {

    @Rule @JvmField
    val folder = TemporaryFolder()

    @Mock private lateinit var client: Client
    private lateinit var browserStore: BrowserStore
    @Mock private lateinit var broadcastManager: LocalBroadcastManager
    private lateinit var service: AbstractFetchDownloadService

    private val testDispatcher = TestCoroutineDispatcher()
    private lateinit var shadowNotificationService: ShadowNotificationManager

    @Before
    fun setup() {
        Dispatchers.setMain(testDispatcher)
        openMocks(this)
        browserStore = BrowserStore()
        service = spy(object : AbstractFetchDownloadService() {
            override val httpClient = client
            override val store = browserStore
        })

        doReturn(broadcastManager).`when`(service).broadcastManager
        doReturn(testContext).`when`(service).context
        doNothing().`when`(service).useFileStream(any(), anyBoolean(), any())

        shadowNotificationService =
            shadowOf(testContext.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager)
    }

    @After
    fun afterEach() {
        Dispatchers.resetMain()
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

        val downloadIntent = Intent("ACTION_DOWNLOAD")
        downloadIntent.putExtra(EXTRA_DOWNLOAD_ID, download.id)

        browserStore.dispatch(DownloadAction.AddDownloadAction(download)).joinBlocking()
        service.onStartCommand(downloadIntent, 0, 0)
        service.downloadJobs.values.forEach { it.job?.join() }

        val providedDownload = argumentCaptor<DownloadJobState>()
        verify(service).performDownload(providedDownload.capture(), anyBoolean())

        assertEquals(download.url, providedDownload.value.state.url)
        assertEquals(download.fileName, providedDownload.value.state.fileName)

        // Ensure the job is properly added to the map
        assertEquals(1, service.downloadJobs.count())
        assertNotNull(service.downloadJobs[providedDownload.value.state.id])
    }

    @Test
    fun `WHEN a download intent is received THEN handleDownloadIntent must be called`() = runBlocking {
        val download = DownloadState("https://example.com/file.txt", "file.txt")
        val downloadIntent = Intent("ACTION_DOWNLOAD")

        doNothing().`when`(service).handleRemovePrivateDownloadIntent(any())
        doNothing().`when`(service).handleDownloadIntent(any())

        downloadIntent.putExtra(EXTRA_DOWNLOAD_ID, download.id)

        browserStore.dispatch(DownloadAction.AddDownloadAction(download)).joinBlocking()

        service.onStartCommand(downloadIntent, 0, 0)

        verify(service).handleDownloadIntent(download)
        verify(service, never()).handleRemovePrivateDownloadIntent(download)
    }

    @Test
    fun `WHEN an intent does not provide an action THEN handleDownloadIntent must be called`() = runBlocking {
        val download = DownloadState("https://example.com/file.txt", "file.txt")
        val downloadIntent = Intent()

        doNothing().`when`(service).handleRemovePrivateDownloadIntent(any())
        doNothing().`when`(service).handleDownloadIntent(any())

        downloadIntent.putExtra(EXTRA_DOWNLOAD_ID, download.id)

        browserStore.dispatch(DownloadAction.AddDownloadAction(download)).joinBlocking()

        service.onStartCommand(downloadIntent, 0, 0)

        verify(service).handleDownloadIntent(download)
        verify(service, never()).handleRemovePrivateDownloadIntent(download)
    }

    @Test
    fun `WHEN a remove download intent is received THEN handleRemoveDownloadIntent must be called`() = runBlocking {
        val download = DownloadState("https://example.com/file.txt", "file.txt")
        val downloadIntent = Intent(ACTION_REMOVE_PRIVATE_DOWNLOAD)

        doNothing().`when`(service).handleRemovePrivateDownloadIntent(any())
        doNothing().`when`(service).handleDownloadIntent(any())

        downloadIntent.putExtra(EXTRA_DOWNLOAD_ID, download.id)

        browserStore.dispatch(DownloadAction.AddDownloadAction(download)).joinBlocking()

        service.onStartCommand(downloadIntent, 0, 0)

        verify(service).handleRemovePrivateDownloadIntent(download)
        verify(service, never()).handleDownloadIntent(download)
    }

    @Test
    fun `WHEN handleRemovePrivateDownloadIntent with a privae download is called THEN removeDownloadJob must be called`() {
        val downloadState = DownloadState(url = "mozilla.org/mozilla.txt", private = true)
        val downloadJobState = DownloadJobState(state = downloadState, status = COMPLETED)
        val browserStore = mock<BrowserStore>()
        val service = spy(object : AbstractFetchDownloadService() {
            override val httpClient = client
            override val store = browserStore
        })

        doAnswer { Unit }.`when`(service).removeDownloadJob(any())

        service.downloadJobs[downloadState.id] = downloadJobState

        service.handleRemovePrivateDownloadIntent(downloadState)

        verify(service).removeDownloadJob(downloadJobState)
        verify(browserStore).dispatch(DownloadAction.RemoveDownloadAction(downloadState.id))
    }

    @Test
    fun `WHEN handleRemovePrivateDownloadIntent is called with with a non-private (or regular) download THEN removeDownloadJob must not be called`() {
        val downloadState = DownloadState(url = "mozilla.org/mozilla.txt", private = false)
        val downloadJobState = DownloadJobState(state = downloadState, status = COMPLETED)
        val browserStore = mock<BrowserStore>()
        val service = spy(object : AbstractFetchDownloadService() {
            override val httpClient = client
            override val store = browserStore
        })

        doAnswer { Unit }.`when`(service).removeDownloadJob(any())

        service.downloadJobs[downloadState.id] = downloadJobState

        service.handleRemovePrivateDownloadIntent(downloadState)

        verify(service, never()).removeDownloadJob(downloadJobState)
        verify(browserStore, never()).dispatch(DownloadAction.RemoveDownloadAction(downloadState.id))
    }

    @Test
    fun `service redelivers if no download extra is passed `() = runBlocking {
        val downloadIntent = Intent("ACTION_DOWNLOAD")

        val intentCode = service.onStartCommand(downloadIntent, 0, 0)

        assertEquals(Service.START_REDELIVER_INTENT, intentCode)
    }

    @Test
    fun `verifyDownload sets the download to failed if it is not complete`() = runBlocking {
        val downloadState = DownloadState(
            url = "mozilla.org/mozilla.txt",
            contentLength = 50L,
            currentBytesCopied = 5,
            status = DOWNLOADING
        )

        val downloadJobState = DownloadJobState(
            job = null,
            state = downloadState,
            foregroundServiceId = 1,
            downloadDeleted = false,
            currentBytesCopied = 5,
            status = DOWNLOADING
        )

        service.verifyDownload(downloadJobState)

        assertEquals(FAILED, service.getDownloadJobStatus(downloadJobState))
        verify(service).setDownloadJobStatus(downloadJobState, FAILED)
        verify(service).updateDownloadState(downloadState.copy(status = FAILED))
    }

    @Test
    fun `verifyDownload does NOT set the download to failed if it is paused`() = runBlocking {
        val downloadState = DownloadState(
            url = "mozilla.org/mozilla.txt",
            contentLength = 50L,
            currentBytesCopied = 5,
            status = DownloadState.Status.PAUSED
        )

        val downloadJobState = DownloadJobState(
            job = null,
            state = downloadState,
            currentBytesCopied = 5,
            status = DownloadState.Status.PAUSED,
            foregroundServiceId = 1,
            downloadDeleted = false
        )

        service.verifyDownload(downloadJobState)

        assertEquals(DownloadState.Status.PAUSED, service.getDownloadJobStatus(downloadJobState))
        verify(service, times(0)).setDownloadJobStatus(downloadJobState, DownloadState.Status.FAILED)
        verify(service, times(0)).updateDownloadState(downloadState.copy(status = DownloadState.Status.FAILED))
    }

    @Test
    fun `verifyDownload does NOT set the download to failed if it is complete`() = runBlocking {
        val downloadState = DownloadState(
            url = "mozilla.org/mozilla.txt",
            contentLength = 50L,
            currentBytesCopied = 50,
            status = DOWNLOADING
        )

        val downloadJobState = DownloadJobState(
            job = null,
            state = downloadState,
            currentBytesCopied = 50,
            status = DOWNLOADING,
            foregroundServiceId = 1,
            downloadDeleted = false
        )

        service.verifyDownload(downloadJobState)

        assertNotEquals(FAILED, service.getDownloadJobStatus(downloadJobState))
        verify(service, times(0)).setDownloadJobStatus(downloadJobState, FAILED)
        verify(service, times(0)).updateDownloadState(downloadState.copy(status = FAILED))
    }

    @Test
    fun `verifyDownload does NOT set the download to failed if it is cancelled`() = runBlocking {
        val downloadState = DownloadState(
            url = "mozilla.org/mozilla.txt",
            contentLength = 50L,
            currentBytesCopied = 50,
            status = DownloadState.Status.CANCELLED
        )

        val downloadJobState = DownloadJobState(
            job = null,
            state = downloadState,
            currentBytesCopied = 50,
            status = DownloadState.Status.CANCELLED,
            foregroundServiceId = 1,
            downloadDeleted = false
        )

        service.verifyDownload(downloadJobState)

        assertNotEquals(FAILED, service.getDownloadJobStatus(downloadJobState))
        verify(service, times(0)).setDownloadJobStatus(downloadJobState, FAILED)
        verify(service, times(0)).updateDownloadState(downloadState.copy(status = FAILED))
    }

    @Test
    fun `verifyDownload does NOT set the download to failed if it is status COMPLETED`() = runBlocking {
        val downloadState = DownloadState(
            url = "mozilla.org/mozilla.txt",
            contentLength = 50L,
            currentBytesCopied = 50,
            status = DownloadState.Status.COMPLETED
        )

        val downloadJobState = DownloadJobState(
            job = null,
            state = downloadState,
            currentBytesCopied = 50,
            status = DownloadState.Status.COMPLETED,
            foregroundServiceId = 1,
            downloadDeleted = false
        )

        service.verifyDownload(downloadJobState)

        verify(service, times(0)).setDownloadJobStatus(downloadJobState, FAILED)
        verify(service, times(0)).updateDownloadState(downloadState.copy(status = FAILED))
    }

    @Test
    fun `verify that a COMPLETED download contains a file size`() {
        val downloadState = DownloadState(
            url = "mozilla.org/mozilla.txt",
            contentLength = 0L,
            currentBytesCopied = 50,
            status = DOWNLOADING
        )
        val downloadJobState = DownloadJobState(
            job = null,
            state = downloadState,
            currentBytesCopied = 50,
            status = DOWNLOADING,
            foregroundServiceId = 1,
            downloadDeleted = false
        )

        browserStore.dispatch(DownloadAction.AddDownloadAction(downloadState)).joinBlocking()
        service.downloadJobs[downloadJobState.state.id] = downloadJobState
        service.verifyDownload(downloadJobState)
        browserStore.waitUntilIdle()

        assertEquals(downloadJobState.state.contentLength, service.downloadJobs[downloadJobState.state.id]!!.state.contentLength)
        assertEquals(downloadJobState.state.contentLength, browserStore.state.downloads.values.first().contentLength)
    }

    @Test
    fun `broadcastReceiver handles ACTION_PAUSE`() = runBlocking {
        val download = DownloadState("https://example.com/file.txt", "file.txt")
        val response = Response(
            "https://example.com/file.txt",
            200,
            MutableHeaders(),
            Response.Body(mock())
        )
        doReturn(response).`when`(client).fetch(Request("https://example.com/file.txt"))

        val downloadIntent = Intent("ACTION_DOWNLOAD")
        downloadIntent.putExtra(EXTRA_DOWNLOAD_ID, download.id)

        browserStore.dispatch(DownloadAction.AddDownloadAction(download)).joinBlocking()
        service.onStartCommand(downloadIntent, 0, 0)
        service.downloadJobs.values.forEach { it.job?.join() }

        val providedDownload = argumentCaptor<DownloadJobState>()
        verify(service).performDownload(providedDownload.capture(), anyBoolean())

        val pauseIntent = Intent(ACTION_PAUSE).apply {
            setPackage(testContext.applicationContext.packageName)
            putExtra(DownloadNotification.EXTRA_DOWNLOAD_ID, providedDownload.value.state.id)
        }

        CollectionProcessor.withFactCollection { facts ->
            service.broadcastReceiver.onReceive(testContext, pauseIntent)

            val pauseFact = facts[0]
            assertEquals(Action.PAUSE, pauseFact.action)
            assertEquals(NOTIFICATION, pauseFact.item)
        }

        service.downloadJobs[providedDownload.value.state.id]?.job?.join()
        val downloadJobState = service.downloadJobs[providedDownload.value.state.id]!!
        assertEquals(DownloadState.Status.PAUSED, service.getDownloadJobStatus(downloadJobState))
    }

    @Test
    fun `broadcastReceiver handles ACTION_CANCEL`() = runBlocking {
        val download = DownloadState("https://example.com/file.txt", "file.txt")
        val response = Response(
            "https://example.com/file.txt",
            200,
            MutableHeaders(),
            Response.Body(mock())
        )
        doReturn(response).`when`(client).fetch(Request("https://example.com/file.txt"))

        val downloadIntent = Intent("ACTION_DOWNLOAD")
        downloadIntent.putExtra(EXTRA_DOWNLOAD_ID, download.id)

        browserStore.dispatch(DownloadAction.AddDownloadAction(download)).joinBlocking()
        service.onStartCommand(downloadIntent, 0, 0)
        service.downloadJobs.values.forEach { it.job?.join() }

        val providedDownload = argumentCaptor<DownloadJobState>()
        verify(service).performDownload(providedDownload.capture(), anyBoolean())

        val cancelIntent = Intent(ACTION_CANCEL).apply {
            setPackage(testContext.applicationContext.packageName)
            putExtra(DownloadNotification.EXTRA_DOWNLOAD_ID, providedDownload.value.state.id)
        }

        assertFalse(service.downloadJobs[providedDownload.value.state.id]!!.downloadDeleted)

        CollectionProcessor.withFactCollection { facts ->
            service.broadcastReceiver.onReceive(testContext, cancelIntent)

            val cancelFact = facts[0]
            assertEquals(Action.CANCEL, cancelFact.action)
            assertEquals(NOTIFICATION, cancelFact.item)
        }
    }

    @Test
    fun `broadcastReceiver handles ACTION_RESUME`() = runBlocking {
        val download = DownloadState("https://example.com/file.txt", "file.txt")

        val downloadResponse = Response(
            "https://example.com/file.txt",
            200,
            MutableHeaders(),
            Response.Body(mock())
        )
        val resumeResponse = Response(
            "https://example.com/file.txt",
            206,
            MutableHeaders("Content-Range" to "1-67589/67589"),
            Response.Body(mock())
        )
        doReturn(downloadResponse).`when`(client)
            .fetch(Request("https://example.com/file.txt"))
        doReturn(resumeResponse).`when`(client)
            .fetch(Request("https://example.com/file.txt", headers = MutableHeaders("Range" to "bytes=1-")))

        val downloadIntent = Intent("ACTION_DOWNLOAD")
        downloadIntent.putExtra(EXTRA_DOWNLOAD_ID, download.id)

        browserStore.dispatch(DownloadAction.AddDownloadAction(download)).joinBlocking()
        service.onStartCommand(downloadIntent, 0, 0)
        service.downloadJobs.values.forEach { it.job?.join() }

        val providedDownload = argumentCaptor<DownloadJobState>()
        verify(service).performDownload(providedDownload.capture(), anyBoolean())

        // Simulate a pause
        var downloadJobState = service.downloadJobs[providedDownload.value.state.id]!!
        downloadJobState.currentBytesCopied = 1
        service.setDownloadJobStatus(downloadJobState, DownloadState.Status.PAUSED)

        service.setDownloadJobStatus(downloadJobState, DownloadState.Status.PAUSED)
        service.downloadJobs[providedDownload.value.state.id]?.job?.cancel()

        val resumeIntent = Intent(ACTION_RESUME).apply {
            setPackage(testContext.applicationContext.packageName)
            putExtra(DownloadNotification.EXTRA_DOWNLOAD_ID, providedDownload.value.state.id)
        }

        CollectionProcessor.withFactCollection { facts ->
            service.broadcastReceiver.onReceive(testContext, resumeIntent)

            val resumeFact = facts[0]
            assertEquals(Action.RESUME, resumeFact.action)
            assertEquals(NOTIFICATION, resumeFact.item)
        }

        downloadJobState = service.downloadJobs[providedDownload.value.state.id]!!
        assertEquals(DOWNLOADING, service.getDownloadJobStatus(downloadJobState))

        // Make sure the download job is completed (break out of copyInChunks)
        service.setDownloadJobStatus(downloadJobState, DownloadState.Status.PAUSED)

        service.downloadJobs[providedDownload.value.state.id]?.job?.join()

        verify(service).startDownloadJob(providedDownload.value)
    }

    @Test
    fun `broadcastReceiver handles ACTION_TRY_AGAIN`() = runBlocking {
        val download = DownloadState("https://example.com/file.txt", "file.txt")
        val response = Response(
            "https://example.com/file.txt",
            200,
            MutableHeaders(),
            Response.Body(mock())
        )
        doReturn(response).`when`(client).fetch(Request("https://example.com/file.txt"))

        val downloadIntent = Intent("ACTION_DOWNLOAD")
        downloadIntent.putExtra(EXTRA_DOWNLOAD_ID, download.id)

        browserStore.dispatch(DownloadAction.AddDownloadAction(download)).joinBlocking()
        service.onStartCommand(downloadIntent, 0, 0)
        service.downloadJobs.values.forEach { it.job?.join() }

        val providedDownload = argumentCaptor<DownloadJobState>()
        verify(service).performDownload(providedDownload.capture(), anyBoolean())
        service.downloadJobs[providedDownload.value.state.id]?.job?.join()

        // Simulate a failure
        var downloadJobState = service.downloadJobs[providedDownload.value.state.id]!!
        service.setDownloadJobStatus(downloadJobState, FAILED)
        service.downloadJobs[providedDownload.value.state.id]?.job?.cancel()

        val tryAgainIntent = Intent(ACTION_TRY_AGAIN).apply {
            setPackage(testContext.applicationContext.packageName)
            putExtra(DownloadNotification.EXTRA_DOWNLOAD_ID, providedDownload.value.state.id)
        }

        CollectionProcessor.withFactCollection { facts ->
            service.broadcastReceiver.onReceive(testContext, tryAgainIntent)

            val tryAgainFact = facts[0]
            assertEquals(Action.TRY_AGAIN, tryAgainFact.action)
            assertEquals(NOTIFICATION, tryAgainFact.item)
        }

        downloadJobState = service.downloadJobs[providedDownload.value.state.id]!!
        assertEquals(DOWNLOADING, service.getDownloadJobStatus(downloadJobState))

        // Make sure the download job is completed (break out of copyInChunks)
        service.setDownloadJobStatus(downloadJobState, DownloadState.Status.PAUSED)

        service.downloadJobs[providedDownload.value.state.id]?.job?.join()

        verify(service).startDownloadJob(providedDownload.value)
    }

    @Test
    fun `download fails on a bad network response`() = runBlocking {
        val download = DownloadState("https://example.com/file.txt", "file.txt")
        val response = Response(
            "https://example.com/file.txt",
            400,
            MutableHeaders(),
            Response.Body(mock())
        )
        doReturn(response).`when`(client).fetch(Request("https://example.com/file.txt"))

        val downloadIntent = Intent("ACTION_DOWNLOAD")
        downloadIntent.putExtra(EXTRA_DOWNLOAD_ID, download.id)

        browserStore.dispatch(DownloadAction.AddDownloadAction(download)).joinBlocking()
        service.onStartCommand(downloadIntent, 0, 0)
        service.downloadJobs.values.forEach { it.job?.join() }

        val providedDownload = argumentCaptor<DownloadJobState>()
        verify(service).performDownload(providedDownload.capture(), anyBoolean())

        service.downloadJobs[providedDownload.value.state.id]?.job?.join()
        val downloadJobState = service.downloadJobs[providedDownload.value.state.id]!!
        assertEquals(FAILED, service.getDownloadJobStatus(downloadJobState))
    }

    @Test
    fun `makeUniqueFileNameIfNecessary transforms fileName when appending FALSE`() {
        folder.newFile("example.apk")

        val download = DownloadState(
            url = "mozilla.org",
            fileName = "example.apk",
            destinationDirectory = folder.root.path
        )
        val transformedDownload = service.makeUniqueFileNameIfNecessary(download, false)

        assertNotEquals(download.fileName, transformedDownload.fileName)
    }

    @Test
    fun `makeUniqueFileNameIfNecessary does NOT transform fileName when appending TRUE`() {
        folder.newFile("example.apk")

        val download = DownloadState(
            url = "mozilla.org",
            fileName = "example.apk",
            destinationDirectory = folder.root.path
        )
        val transformedDownload = service.makeUniqueFileNameIfNecessary(download, true)

        assertEquals(download, transformedDownload)
    }

    @Test
    fun `notification is shown when download status is ACTIVE`() = runBlocking {
        val download = DownloadState("https://example.com/file.txt", "file.txt")
        val response = Response(
            "https://example.com/file.txt",
            200,
            MutableHeaders(),
            Response.Body(mock())
        )
        doReturn(response).`when`(client).fetch(Request("https://example.com/file.txt"))

        val downloadIntent = Intent("ACTION_DOWNLOAD")
        downloadIntent.putExtra(EXTRA_DOWNLOAD_ID, download.id)

        browserStore.dispatch(DownloadAction.AddDownloadAction(download)).joinBlocking()
        service.onStartCommand(downloadIntent, 0, 0)
        service.downloadJobs.values.forEach { it.job?.join() }

        val providedDownload = argumentCaptor<DownloadJobState>()
        verify(service).performDownload(providedDownload.capture(), anyBoolean())

        service.downloadJobs[providedDownload.value.state.id]?.job?.join()
        val downloadJobState = service.downloadJobs[providedDownload.value.state.id]!!
        service.setDownloadJobStatus(downloadJobState, DOWNLOADING)
        assertEquals(DOWNLOADING, service.getDownloadJobStatus(downloadJobState))

        testDispatcher.advanceTimeBy(PROGRESS_UPDATE_INTERVAL)

        // The additional notification is the summary one (the notification group).
        assertEquals(2, shadowNotificationService.size())
    }

    @Test
    fun `onStartCommand must change status of INITIATED downloads to DOWNLOADING`() = runBlocking {
        val download = DownloadState("https://example.com/file.txt", "file.txt", status = INITIATED)

        val downloadIntent = Intent("ACTION_DOWNLOAD")
        downloadIntent.putExtra(EXTRA_DOWNLOAD_ID, download.id)

        doNothing().`when`(service).performDownload(any(), anyBoolean())

        browserStore.dispatch(DownloadAction.AddDownloadAction(download)).joinBlocking()
        service.onStartCommand(downloadIntent, 0, 0)
        service.downloadJobs.values.first().job!!.joinBlocking()

        verify(service).startDownloadJob(any())
        assertEquals(DOWNLOADING, service.downloadJobs.values.first().status)
    }

    @Test
    fun `onStartCommand must change the status only for INITIATED downloads`() = runBlocking {
        val download = DownloadState("https://example.com/file.txt", "file.txt", status = FAILED)

        val downloadIntent = Intent("ACTION_DOWNLOAD")
        downloadIntent.putExtra(EXTRA_DOWNLOAD_ID, download.id)

        browserStore.dispatch(DownloadAction.AddDownloadAction(download)).joinBlocking()
        service.onStartCommand(downloadIntent, 0, 0)

        verify(service, never()).startDownloadJob(any())
        assertEquals(FAILED, service.downloadJobs.values.first().status)
    }

    @Test
    fun `onStartCommand sets the notification foreground`() = runBlocking {
        val download = DownloadState("https://example.com/file.txt", "file.txt")

        val downloadIntent = Intent("ACTION_DOWNLOAD")
        downloadIntent.putExtra(EXTRA_DOWNLOAD_ID, download.id)

        doNothing().`when`(service).performDownload(any(), anyBoolean())

        browserStore.dispatch(DownloadAction.AddDownloadAction(download)).joinBlocking()
        service.onStartCommand(downloadIntent, 0, 0)

        verify(service).setForegroundNotification(any())
    }

    @Test
    fun `sets the notification foreground in devices that support notification group`() = runBlocking {
        val download = DownloadState(
            id = "1", url = "https://example.com/file.txt", fileName = "file.txt",
            status = DOWNLOADING
        )
        val downloadState = DownloadJobState(
            state = download,
            status = DOWNLOADING,
            foregroundServiceId = Random.nextInt()
        )
        val notification = mock<Notification>()

        doReturn(notification).`when`(service).updateNotificationGroup()

        service.downloadJobs["1"] = downloadState

        service.setForegroundNotification(downloadState)

        verify(service).startForeground(NOTIFICATION_DOWNLOAD_GROUP_ID, notification)
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.M])
    fun `sets the notification foreground in devices that DO NOT support notification group`() {
        val download = DownloadState(
            id = "1", url = "https://example.com/file.txt",
            fileName = "file.txt", status = DOWNLOADING
        )
        val downloadState = DownloadJobState(
            state = download,
            status = DOWNLOADING,
            foregroundServiceId = Random.nextInt()
        )
        val notification = mock<Notification>()

        doReturn(notification).`when`(service).createCompactForegroundNotification(downloadState)

        service.downloadJobs["1"] = downloadState

        service.setForegroundNotification(downloadState)

        verify(service).startForeground(downloadState.foregroundServiceId, notification)
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.M])
    fun createCompactForegroundNotification() {
        val download = DownloadState(
            id = "1", url = "https://example.com/file.txt", fileName = "file.txt",
            status = DOWNLOADING
        )
        val downloadState = DownloadJobState(
            state = download,
            status = DOWNLOADING,
            foregroundServiceId = Random.nextInt()
        )

        assertEquals(0, shadowNotificationService.size())

        val notification = service.createCompactForegroundNotification(downloadState)

        service.downloadJobs["1"] = downloadState

        service.setForegroundNotification(downloadState)

        assertNull(notification.group)
        assertEquals(1, shadowNotificationService.size())
        assertNotNull(shadowNotificationService.getNotification(downloadState.foregroundServiceId))
    }

    @Test
    fun `getForegroundId in devices that support notification group will return NOTIFICATION_DOWNLOAD_GROUP_ID`() {
        val download = DownloadState(id = "1", url = "https://example.com/file.txt", fileName = "file.txt")

        val downloadIntent = Intent("ACTION_DOWNLOAD")
        downloadIntent.putExtra(EXTRA_DOWNLOAD_ID, download.id)

        doNothing().`when`(service).performDownload(any(), anyBoolean())

        service.onStartCommand(downloadIntent, 0, 0)

        assertEquals(NOTIFICATION_DOWNLOAD_GROUP_ID, service.getForegroundId())
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.M])
    fun `getForegroundId in devices that support DO NOT notification group will return the latest active download`() {
        val download = DownloadState(id = "1", url = "https://example.com/file.txt", fileName = "file.txt")

        val downloadIntent = Intent("ACTION_DOWNLOAD")
        downloadIntent.putExtra(EXTRA_DOWNLOAD_ID, download.id)

        doNothing().`when`(service).performDownload(any(), anyBoolean())

        browserStore.dispatch(DownloadAction.AddDownloadAction(download)).joinBlocking()
        service.onStartCommand(downloadIntent, 0, 0)

        val foregroundId = service.downloadJobs.values.first().foregroundServiceId
        assertEquals(foregroundId, service.getForegroundId())
        assertEquals(foregroundId, service.compatForegroundNotificationId)
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.M])
    fun `updateNotificationGroup will do nothing on devices that do not support notificaiton groups`() = runBlocking {
        val download = DownloadState(
            id = "1", url = "https://example.com/file.txt", fileName = "file.txt",
            status = DOWNLOADING
        )
        val downloadState = DownloadJobState(
            state = download,
            status = DOWNLOADING,
            foregroundServiceId = Random.nextInt()
        )

        service.downloadJobs["1"] = downloadState

        val notificationGroup = service.updateNotificationGroup()

        assertNull(notificationGroup)
        assertEquals(0, shadowNotificationService.size())
    }

    @Test
    fun `removeDownloadJob will update the background notification if there are other pending downloads`() {
        val download = DownloadState(
            id = "1", url = "https://example.com/file.txt", fileName = "file.txt",
            status = DOWNLOADING
        )
        val downloadState = DownloadJobState(
            state = download,
            status = DOWNLOADING,
            foregroundServiceId = Random.nextInt()
        )

        service.downloadJobs["1"] = downloadState
        service.downloadJobs["2"] = mock()

        doNothing().`when`(service).updateForegroundNotificationIfNeeded(downloadState)

        service.removeDownloadJob(downloadJobState = downloadState)

        assertEquals(1, service.downloadJobs.size)
        verify(service).updateForegroundNotificationIfNeeded(downloadState)
        verify(service).removeNotification(testContext, downloadState)
    }

    @Test
    fun `WHEN all downloads are completed stopForeground must be called`() {
        val download1 = DownloadState(
            id = "1", url = "https://example.com/file1.txt", fileName = "file1.txt",
            status = COMPLETED
        )
        val download2 = DownloadState(
            id = "2", url = "https://example.com/file2.txt", fileName = "file2.txt",
            status = COMPLETED
        )
        val downloadState1 = DownloadJobState(
            state = download1,
            status = COMPLETED,
            foregroundServiceId = Random.nextInt()
        )

        val downloadState2 = DownloadJobState(
            state = download2,
            status = COMPLETED,
            foregroundServiceId = Random.nextInt()
        )

        service.downloadJobs["1"] = downloadState1
        service.downloadJobs["2"] = downloadState2

        service.updateForegroundNotificationIfNeeded(downloadState1)

        verify(service).stopForeground(false)
    }

    @Test
    fun `Until all downloads are NOT completed stopForeground must NOT be called`() {
        val download1 = DownloadState(
            id = "1", url = "https://example.com/file1.txt", fileName = "file1.txt",
            status = COMPLETED
        )
        val download2 = DownloadState(
            id = "2", url = "https://example.com/file2.txt", fileName = "file2.txt",
            status = DOWNLOADING
        )
        val downloadState1 = DownloadJobState(
            state = download1,
            status = COMPLETED,
            foregroundServiceId = Random.nextInt()
        )

        val downloadState2 = DownloadJobState(
            state = download2,
            status = DOWNLOADING,
            foregroundServiceId = Random.nextInt()
        )

        service.downloadJobs["1"] = downloadState1
        service.downloadJobs["2"] = downloadState2

        service.updateForegroundNotificationIfNeeded(downloadState1)

        verify(service, never()).stopForeground(false)
    }

    @Test
    fun `removeDownloadJob will stop the service if there are none pending downloads`() {
        val download = DownloadState(
            id = "1", url = "https://example.com/file.txt", fileName = "file.txt",
            status = DOWNLOADING
        )
        val downloadState = DownloadJobState(
            state = download,
            status = DOWNLOADING,
            foregroundServiceId = Random.nextInt()
        )

        doNothing().`when`(service).stopForeground(false)
        doNothing().`when`(service).clearAllDownloadsNotificationsAndJobs()
        doNothing().`when`(service).stopSelf()

        service.downloadJobs["1"] = downloadState

        service.removeDownloadJob(downloadJobState = downloadState)

        assertTrue(service.downloadJobs.isEmpty())
        verify(service).stopSelf()
        verify(service, times(0)).updateForegroundNotificationIfNeeded(downloadState)
    }

    @Test
    fun `updateForegroundNotification will update the notification group for devices that support it`() {
        doReturn(null).`when`(service).updateNotificationGroup()

        service.updateForegroundNotificationIfNeeded(mock())

        verify(service).updateNotificationGroup()
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.M])
    fun `updateForegroundNotification will select a new foreground notification`() {
        val downloadState1 = DownloadJobState(
            state = DownloadState(
                id = "1", url = "https://example.com/file.txt", fileName = "file.txt",
                status = DownloadState.Status.COMPLETED
            ),
            status = DownloadState.Status.COMPLETED,
            foregroundServiceId = Random.nextInt()
        )
        val downloadState2 = DownloadJobState(
            state = DownloadState(
                id = "2", url = "https://example.com/file.txt", fileName = "file.txt",
                status = DOWNLOADING
            ),
            status = DOWNLOADING,
            foregroundServiceId = Random.nextInt()
        )

        service.compatForegroundNotificationId = downloadState1.foregroundServiceId

        service.downloadJobs["1"] = downloadState1
        service.downloadJobs["2"] = downloadState2

        service.updateForegroundNotificationIfNeeded(downloadState1)

        verify(service).setForegroundNotification(downloadState2)
        assertEquals(downloadState2.foregroundServiceId, service.compatForegroundNotificationId)
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.M])
    fun `updateForegroundNotification will NOT select a new foreground notification`() {
        val downloadState1 = DownloadJobState(
            state = DownloadState(
                id = "1", url = "https://example.com/file.txt", fileName = "file.txt",
                status = DOWNLOADING
            ),
            status = DOWNLOADING,
            foregroundServiceId = Random.nextInt()
        )
        val downloadState2 = DownloadJobState(
            state = DownloadState(
                id = "1", url = "https://example.com/file.txt", fileName = "file.txt",
                status = DOWNLOADING
            ),
            status = DOWNLOADING,
            foregroundServiceId = Random.nextInt()
        )

        service.compatForegroundNotificationId = downloadState1.foregroundServiceId

        service.downloadJobs["1"] = downloadState1
        service.downloadJobs["2"] = downloadState2

        service.updateForegroundNotificationIfNeeded(downloadState1)

        verify(service, times(0)).setForegroundNotification(downloadState2)
        verify(service, times(0)).updateNotificationGroup()
        assertEquals(downloadState1.foregroundServiceId, service.compatForegroundNotificationId)
    }

    @Test
    fun `notification is shown when download status is PAUSED`() = runBlocking {
        val download = DownloadState("https://example.com/file.txt", "file.txt")
        val response = Response(
            "https://example.com/file.txt",
            200,
            MutableHeaders(),
            Response.Body(mock())
        )
        doReturn(response).`when`(client).fetch(Request("https://example.com/file.txt"))

        val downloadIntent = Intent("ACTION_DOWNLOAD")
        downloadIntent.putExtra(EXTRA_DOWNLOAD_ID, download.id)

        browserStore.dispatch(DownloadAction.AddDownloadAction(download)).joinBlocking()
        service.onStartCommand(downloadIntent, 0, 0)
        service.downloadJobs.values.forEach { it.job?.join() }

        val providedDownload = argumentCaptor<DownloadJobState>()
        verify(service).performDownload(providedDownload.capture(), anyBoolean())

        service.downloadJobs[providedDownload.value.state.id]?.job?.join()
        val downloadJobState = service.downloadJobs[providedDownload.value.state.id]!!
        service.setDownloadJobStatus(downloadJobState, DownloadState.Status.PAUSED)
        assertEquals(DownloadState.Status.PAUSED, service.getDownloadJobStatus(downloadJobState))

        testDispatcher.advanceTimeBy(PROGRESS_UPDATE_INTERVAL)

        // one of the notifications it is the group notification only for devices the support it
        assertEquals(2, shadowNotificationService.size())
    }

    @Test
    fun `notification is shown when download status is COMPLETED`() = runBlocking {
        val download = DownloadState("https://example.com/file.txt", "file.txt")
        val response = Response(
            "https://example.com/file.txt",
            200,
            MutableHeaders(),
            Response.Body(mock())
        )
        doReturn(response).`when`(client).fetch(Request("https://example.com/file.txt"))

        val downloadIntent = Intent("ACTION_DOWNLOAD")
        downloadIntent.putExtra(EXTRA_DOWNLOAD_ID, download.id)

        browserStore.dispatch(DownloadAction.AddDownloadAction(download)).joinBlocking()
        service.onStartCommand(downloadIntent, 0, 0)
        service.downloadJobs.values.forEach { it.job?.join() }

        val providedDownload = argumentCaptor<DownloadJobState>()
        verify(service).performDownload(providedDownload.capture(), anyBoolean())

        service.downloadJobs[providedDownload.value.state.id]?.job?.join()
        val downloadJobState = service.downloadJobs[providedDownload.value.state.id]!!
        service.setDownloadJobStatus(downloadJobState, DownloadState.Status.COMPLETED)
        assertEquals(DownloadState.Status.COMPLETED, service.getDownloadJobStatus(downloadJobState))

        testDispatcher.advanceTimeBy(PROGRESS_UPDATE_INTERVAL)

        assertEquals(2, shadowNotificationService.size())
    }

    @Test
    fun `notification is shown when download status is FAILED`() = runBlocking {
        val download = DownloadState("https://example.com/file.txt", "file.txt")
        val response = Response(
            "https://example.com/file.txt",
            200,
            MutableHeaders(),
            Response.Body(mock())
        )
        doReturn(response).`when`(client).fetch(Request("https://example.com/file.txt"))

        val downloadIntent = Intent("ACTION_DOWNLOAD")
        downloadIntent.putExtra(EXTRA_DOWNLOAD_ID, download.id)

        browserStore.dispatch(DownloadAction.AddDownloadAction(download)).joinBlocking()
        service.onStartCommand(downloadIntent, 0, 0)
        service.downloadJobs.values.forEach { it.job?.join() }

        val providedDownload = argumentCaptor<DownloadJobState>()
        verify(service).performDownload(providedDownload.capture(), anyBoolean())

        service.downloadJobs[providedDownload.value.state.id]?.job?.join()
        val downloadJobState = service.downloadJobs[providedDownload.value.state.id]!!
        service.setDownloadJobStatus(downloadJobState, FAILED)
        assertEquals(FAILED, service.getDownloadJobStatus(downloadJobState))

        testDispatcher.advanceTimeBy(PROGRESS_UPDATE_INTERVAL)

        // one of the notifications it is the group notification only for devices the support it
        assertEquals(2, shadowNotificationService.size())
    }

    @Test
    fun `notification is not shown when download status is CANCELLED`() = runBlocking {
        val download = DownloadState("https://example.com/file.txt", "file.txt")
        val response = Response(
            "https://example.com/file.txt",
            200,
            MutableHeaders(),
            Response.Body(mock())
        )
        doReturn(response).`when`(client).fetch(Request("https://example.com/file.txt"))

        val downloadIntent = Intent("ACTION_DOWNLOAD")
        downloadIntent.putExtra(EXTRA_DOWNLOAD_ID, download.id)

        browserStore.dispatch(DownloadAction.AddDownloadAction(download)).joinBlocking()
        service.onStartCommand(downloadIntent, 0, 0)
        service.downloadJobs.values.forEach { it.job?.join() }

        val providedDownload = argumentCaptor<DownloadJobState>()
        verify(service).performDownload(providedDownload.capture(), anyBoolean())

        service.downloadJobs[providedDownload.value.state.id]?.job?.join()
        val downloadJobState = service.downloadJobs[providedDownload.value.state.id]!!
        service.setDownloadJobStatus(downloadJobState, DownloadState.Status.CANCELLED)
        assertEquals(DownloadState.Status.CANCELLED, service.getDownloadJobStatus(downloadJobState))

        testDispatcher.advanceTimeBy(PROGRESS_UPDATE_INTERVAL)

        // The additional notification is the summary one (the notification group).
        assertEquals(1, shadowNotificationService.size())
    }

    @Test
    fun `job status is set to failed when an Exception is thrown while performDownload`() = runBlocking {
        doThrow(IOException()).`when`(client).fetch(any())
        val download = DownloadState("https://example.com/file.txt", "file.txt")

        val downloadIntent = Intent("ACTION_DOWNLOAD")
        downloadIntent.putExtra(EXTRA_DOWNLOAD_ID, download.id)

        browserStore.dispatch(DownloadAction.AddDownloadAction(download)).joinBlocking()
        service.onStartCommand(downloadIntent, 0, 0)
        service.downloadJobs.values.forEach { it.job?.join() }

        val providedDownload = argumentCaptor<DownloadJobState>()
        verify(service).performDownload(providedDownload.capture(), anyBoolean())

        val downloadJobState = service.downloadJobs[providedDownload.value.state.id]!!
        assertEquals(FAILED, service.getDownloadJobStatus(downloadJobState))
    }

    @Test
    fun `WHEN a download is from a private session the request must be private`() = runBlocking {
        val response = Response(
            "https://example.com/file.txt",
            200,
            MutableHeaders(),
            Response.Body(mock())
        )
        doReturn(response).`when`(client).fetch(any())
        val download = DownloadState("https://example.com/file.txt", "file.txt", private = true)
        val downloadJob = DownloadJobState(state = download, status = DOWNLOADING)
        val providedRequest = argumentCaptor<Request>()

        service.performDownload(downloadJob)
        verify(client).fetch(providedRequest.capture())
        assertTrue(providedRequest.value.private)

        downloadJob.state = download.copy(private = false)
        service.performDownload(downloadJob)

        verify(client, times(2)).fetch(providedRequest.capture())

        assertFalse(providedRequest.value.private)
    }

    @Test
    fun `performDownload - use the download response when available`() {
        val responseFromDownloadState = mock<Response>()
        val responseFromClient = mock<Response>()
        val download = DownloadState("https://example.com/file.txt", "file.txt", response = responseFromDownloadState)
        val downloadJob = DownloadJobState(state = download, status = DOWNLOADING)

        doReturn(404).`when`(responseFromDownloadState).status
        doReturn(responseFromClient).`when`(client).fetch(any())

        service.performDownload(downloadJob)

        verify(responseFromDownloadState, atLeastOnce()).status
        verifyNoInteractions(client)
    }

    @Test
    fun `performDownload - use the client response when the download response NOT available`() {
        val responseFromClient = mock<Response>()
        val download = spy(DownloadState("https://example.com/file.txt", "file.txt", response = null))
        val downloadJob = DownloadJobState(state = download, status = DOWNLOADING)

        doReturn(404).`when`(responseFromClient).status
        doReturn(responseFromClient).`when`(client).fetch(any())

        service.performDownload(downloadJob)

        verify(responseFromClient, atLeastOnce()).status
    }

    @Test
    fun `performDownload - use the client response when resuming a download`() {
        val responseFromDownloadState = mock<Response>()
        val responseFromClient = mock<Response>()
        val download = spy(DownloadState("https://example.com/file.txt", "file.txt", response = responseFromDownloadState))
        val downloadJob = DownloadJobState(currentBytesCopied = 100, state = download, status = DOWNLOADING)

        doReturn(404).`when`(responseFromClient).status
        doReturn(responseFromClient).`when`(client).fetch(any())

        service.performDownload(downloadJob)

        verify(responseFromClient, atLeastOnce()).status
        verifyNoInteractions(responseFromDownloadState)
    }

    @Test
    fun `onDestroy cancels all running jobs`() = runBlocking {
        val download = DownloadState("https://example.com/file.txt", "file.txt")
        val response = Response(
            "https://example.com/file.txt",
            200,
            MutableHeaders(),
            // Simulate a long running reading operation by sleeping for 5 seconds.
            Response.Body(object : InputStream() {
                override fun read(): Int {
                    Thread.sleep(5000)
                    return 0
                }
            })
        )
        // Call the real method to force the reading of the response's body.
        doCallRealMethod().`when`(service).useFileStream(any(), anyBoolean(), any())
        doReturn(response).`when`(client).fetch(Request("https://example.com/file.txt"))

        val downloadIntent = Intent("ACTION_DOWNLOAD")
        downloadIntent.putExtra(EXTRA_DOWNLOAD_ID, download.id)

        browserStore.dispatch(DownloadAction.AddDownloadAction(download)).joinBlocking()
        service.registerNotificationActionsReceiver()
        service.onStartCommand(downloadIntent, 0, 0)

        service.downloadJobs.values.forEach { assertTrue(it.job!!.isActive) }

        val providedDownload = argumentCaptor<DownloadJobState>()
        verify(service).performDownload(providedDownload.capture(), anyBoolean())

        // Advance the clock so that the puller posts a notification.
        testDispatcher.advanceTimeBy(PROGRESS_UPDATE_INTERVAL)
        // One of the notifications it is the group notification only for devices the support it
        assertEquals(2, shadowNotificationService.size())

        // Now destroy
        service.onDestroy()

        // Assert that jobs were cancelled rather than completed.
        service.downloadJobs.values.forEach {
            assertTrue(it.job!!.isCancelled)
            assertFalse(it.job!!.isCompleted)
        }

        // Assert that all currently shown notifications are gone.
        assertEquals(0, shadowNotificationService.size())
    }

    @Test
    fun `updateDownloadState must update the download state in the store and in the downloadJobs`() {
        val download = DownloadState(
            "https://example.com/file.txt", "file1.txt",
            status = DOWNLOADING
        )
        val downloadJob = DownloadJobState(state = mock(), status = DOWNLOADING)
        val mockStore = mock<BrowserStore>()
        val service = spy(object : AbstractFetchDownloadService() {
            override val httpClient = client
            override val store = mockStore
        })

        service.downloadJobs[download.id] = downloadJob

        service.updateDownloadState(download)

        assertEquals(download, service.downloadJobs[download.id]!!.state)
        verify(mockStore).dispatch(DownloadAction.UpdateDownloadAction(download))
    }

    @Test
    fun `onTaskRemoved cancels all notifications on the shadow notification manager`() = runBlocking {
        val download = DownloadState("https://example.com/file.txt", "file.txt")
        val response = Response(
            "https://example.com/file.txt",
            200,
            MutableHeaders(),
            Response.Body(mock())
        )
        doReturn(response).`when`(client).fetch(Request("https://example.com/file.txt"))

        val downloadIntent = Intent("ACTION_DOWNLOAD")
        downloadIntent.putExtra(EXTRA_DOWNLOAD_ID, download.id)

        browserStore.dispatch(DownloadAction.AddDownloadAction(download)).joinBlocking()
        service.registerNotificationActionsReceiver()
        service.onStartCommand(downloadIntent, 0, 0)
        service.downloadJobs.values.forEach { it.job?.join() }

        val providedDownload = argumentCaptor<DownloadJobState>()
        verify(service).performDownload(providedDownload.capture(), anyBoolean())

        service.setDownloadJobStatus(service.downloadJobs[download.id]!!, DownloadState.Status.PAUSED)

        // Advance the clock so that the poller posts a notification.
        testDispatcher.advanceTimeBy(PROGRESS_UPDATE_INTERVAL)
        assertEquals(2, shadowNotificationService.size())

        // Now simulate onTaskRemoved.
        service.onTaskRemoved(null)

        verify(service).stopSelf()
    }

    @Test
    fun `clearAllDownloadsNotificationsAndJobs cancels all running jobs and remove all notifications`() = runBlocking {
        val download = DownloadState(
            id = "1", url = "https://example.com/file.txt", fileName = "file.txt",
            status = DOWNLOADING
        )
        val downloadState = DownloadJobState(
            state = download,
            foregroundServiceId = Random.nextInt(),
            status = DOWNLOADING,
            job = CoroutineScope(IO).launch { while (true) { } }
        )

        service.registerNotificationActionsReceiver()
        service.downloadJobs[download.id] = downloadState

        val notificationStyle = AbstractFetchDownloadService.Style()
        val notification = DownloadNotification.createOngoingDownloadNotification(
            testContext,
            downloadState,
            notificationStyle.notificationAccentColor
        )

        NotificationManagerCompat.from(testContext).notify(downloadState.foregroundServiceId, notification)

        // We have a pending notification
        assertEquals(1, shadowNotificationService.size())

        service.clearAllDownloadsNotificationsAndJobs()

        // Assert that all currently shown notifications are gone.
        assertEquals(0, shadowNotificationService.size())

        // Assert that jobs were cancelled rather than completed.
        service.downloadJobs.values.forEach {
            assertTrue(it.job!!.isCancelled)
            assertFalse(it.job!!.isCompleted)
        }
    }

    @Test
    fun `onDestroy will remove all download notifications, jobs and will call unregisterNotificationActionsReceiver`() = runBlocking {
        val service = spy(object : AbstractFetchDownloadService() {
            override val httpClient = client
            override val store = browserStore
        })

        doReturn(testContext).`when`(service).context

        service.registerNotificationActionsReceiver()

        service.onDestroy()

        verify(service).clearAllDownloadsNotificationsAndJobs()
        verify(service).unregisterNotificationActionsReceiver()
    }

    @Test
    fun `register and unregister notification actions receiver`() {
        val service = spy(object : AbstractFetchDownloadService() {
            override val httpClient = client
            override val store = browserStore
        })

        doReturn(testContext).`when`(service).context

        service.onCreate()

        verify(service).registerNotificationActionsReceiver()

        service.onDestroy()

        verify(service).unregisterNotificationActionsReceiver()
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.P], shadows = [ShadowFileProvider::class])
    fun `WHEN a download is completed and the scoped storage is not used it MUST be added manually to the download system database`() = runBlockingTest {
        val download = DownloadState(
            url = "http://www.mozilla.org",
            fileName = "example.apk",
            destinationDirectory = folder.root.path,
            status = DownloadState.Status.COMPLETED
        )
        val service = spy(object : AbstractFetchDownloadService() {
            override val httpClient = client
            override val store = browserStore
        })

        val downloadJobState = DownloadJobState(state = download, status = DownloadState.Status.COMPLETED)

        doReturn(testContext).`when`(service).context
        service.updateDownloadNotification(DownloadState.Status.COMPLETED, downloadJobState, this)

        verify(service).addCompletedDownload(
            title = any(),
            description = any(),
            isMediaScannerScannable = eq(true),
            mimeType = any(),
            path = any(),
            length = anyLong(),
            showNotification = anyBoolean(),
            uri = any(),
            referer = any()
        )
    }

    @Test
    fun `WHEN a download is completed and the scoped storage is used addToDownloadSystemDatabaseCompat MUST NOT be called`() = runBlockingTest {
        val download = DownloadState(
            url = "http://www.mozilla.org",
            fileName = "example.apk",
            destinationDirectory = folder.root.path,
            status = DownloadState.Status.COMPLETED
        )
        val service = spy(object : AbstractFetchDownloadService() {
            override val httpClient = client
            override val store = browserStore
        })

        val downloadJobState = DownloadJobState(state = download, status = DownloadState.Status.COMPLETED)

        doReturn(testContext).`when`(service).context
        doNothing().`when`(service).addCompletedDownload(
            title = any(),
            description = any(),
            isMediaScannerScannable = eq(true),
            mimeType = any(),
            path = any(),
            length = anyLong(),
            showNotification = anyBoolean(),
            uri = any(),
            referer = any()
        )
        doReturn(true).`when`(service).shouldUseScopedStorage()

        service.updateDownloadNotification(DownloadState.Status.COMPLETED, downloadJobState, this)

        verify(service, never()).addCompletedDownload(
            title = any(),
            description = any(),
            isMediaScannerScannable = eq(true),
            mimeType = any(),
            path = any(),
            length = anyLong(),
            showNotification = anyBoolean(),
            uri = any(),
            referer = any()
        )
    }

    @Test
    fun `WHEN we download on devices with version higher than Q THEN we use scoped storage`() {
        val service = spy(object : AbstractFetchDownloadService() {
            override val httpClient = client
            override val store = browserStore
        })
        val uniqueFile: DownloadState = mock()
        val qSdkVersion = 29
        doReturn(uniqueFile).`when`(service).makeUniqueFileNameIfNecessary(any(), anyBoolean())
        doNothing().`when`(service).updateDownloadState(uniqueFile)
        doNothing().`when`(service).useFileStreamScopedStorage(eq(uniqueFile), any())
        doReturn(qSdkVersion).`when`(service).getSdkVersion()

        service.useFileStream(mock(), true) {}

        verify(service).useFileStreamScopedStorage(eq(uniqueFile), any())
    }

    @Test
    fun `WHEN we download on devices with version lower than Q THEN we use legacy file stream`() {
        val service = spy(object : AbstractFetchDownloadService() {
            override val httpClient = client
            override val store = browserStore
        })
        val uniqueFile: DownloadState = mock()
        val qSdkVersion = 27
        doReturn(uniqueFile).`when`(service).makeUniqueFileNameIfNecessary(any(), anyBoolean())
        doNothing().`when`(service).updateDownloadState(uniqueFile)
        doNothing().`when`(service).useFileStreamLegacy(eq(uniqueFile), anyBoolean(), any())
        doReturn(qSdkVersion).`when`(service).getSdkVersion()

        service.useFileStream(mock(), true) {}

        verify(service).useFileStreamLegacy(eq(uniqueFile), anyBoolean(), any())
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.P], shadows = [ShadowFileProvider::class])
    @Suppress("Deprecation")
    fun `do not pass non-http(s) url to addCompletedDownload`() = runBlockingTest {
        val download = DownloadState(
            url = "blob:moz-extension://d5ea9baa-64c9-4c3d-bb38-49308c47997c/",
            fileName = "example.apk",
            destinationDirectory = folder.root.path
        )

        val service = spy(object : AbstractFetchDownloadService() {
            override val httpClient = client
            override val store = browserStore
        })

        val spyContext = spy(testContext)
        val downloadManager: DownloadManager = mock()

        doReturn(spyContext).`when`(service).context
        doReturn(downloadManager).`when`(spyContext).getSystemService<DownloadManager>()

        service.addToDownloadSystemDatabaseCompat(download, this)
        verify(downloadManager).addCompletedDownload(anyString(), anyString(), anyBoolean(), anyString(), anyString(), anyLong(), anyBoolean(), isNull(), any())
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.P], shadows = [ShadowFileProvider::class])
    @Suppress("Deprecation")
    fun `pass http(s) url to addCompletedDownload`() = runBlockingTest {
        val download = DownloadState(
            url = "https://mozilla.com",
            fileName = "example.apk",
            destinationDirectory = folder.root.path
        )

        val service = spy(object : AbstractFetchDownloadService() {
            override val httpClient = client
            override val store = browserStore
        })

        val spyContext = spy(testContext)
        val downloadManager: DownloadManager = mock()

        doReturn(spyContext).`when`(service).context
        doReturn(downloadManager).`when`(spyContext).getSystemService<DownloadManager>()

        service.addToDownloadSystemDatabaseCompat(download, this)
        verify(downloadManager).addCompletedDownload(anyString(), anyString(), anyBoolean(), anyString(), anyString(), anyLong(), anyBoolean(), any(), any())
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.P], shadows = [ShadowFileProvider::class])
    @Suppress("Deprecation")
    fun `always call addCompletedDownload with a not empty or null mimeType`() = runBlockingTest {
        val service = spy(object : AbstractFetchDownloadService() {
            override val httpClient = client
            override val store = browserStore
        })
        val spyContext = spy(testContext)
        var downloadManager: DownloadManager = mock()
        doReturn(spyContext).`when`(service).context
        doReturn(downloadManager).`when`(spyContext).getSystemService<DownloadManager>()
        val downloadWithNullMimeType = DownloadState(
            url = "blob:moz-extension://d5ea9baa-64c9-4c3d-bb38-49308c47997c/",
            fileName = "example.apk",
            destinationDirectory = folder.root.path,
            contentType = null
        )
        val downloadWithEmptyMimeType = downloadWithNullMimeType.copy(contentType = "")
        val defaultMimeType = "*/*"

        service.addToDownloadSystemDatabaseCompat(downloadWithNullMimeType, this)
        verify(downloadManager).addCompletedDownload(
            anyString(), anyString(), anyBoolean(), eq(defaultMimeType),
            anyString(), anyLong(), anyBoolean(), isNull(), any()
        )

        downloadManager = mock()
        doReturn(downloadManager).`when`(spyContext).getSystemService<DownloadManager>()
        service.addToDownloadSystemDatabaseCompat(downloadWithEmptyMimeType, this)
        verify(downloadManager).addCompletedDownload(
            anyString(), anyString(), anyBoolean(), eq(defaultMimeType),
            anyString(), anyLong(), anyBoolean(), isNull(), any()
        )
    }

    @Test
    fun `cancelled download does not prevent other notifications`() = runBlocking {
        val cancelledDownload = DownloadState("https://example.com/file.txt", "file.txt")
        val response = Response(
            "https://example.com/file.txt",
            200,
            MutableHeaders(),
            Response.Body(mock())
        )

        doReturn(response).`when`(client).fetch(Request("https://example.com/file.txt"))
        val cancelledDownloadIntent = Intent("ACTION_DOWNLOAD")
        cancelledDownloadIntent.putExtra(EXTRA_DOWNLOAD_ID, cancelledDownload.id)

        browserStore.dispatch(DownloadAction.AddDownloadAction(cancelledDownload)).joinBlocking()
        service.onStartCommand(cancelledDownloadIntent, 0, 0)
        service.downloadJobs.values.forEach { it.job?.join() }

        val providedDownload = argumentCaptor<DownloadJobState>()

        verify(service).performDownload(providedDownload.capture(), anyBoolean())
        service.downloadJobs[providedDownload.value.state.id]?.job?.join()

        val cancelledDownloadJobState = service.downloadJobs[providedDownload.value.state.id]!!

        service.setDownloadJobStatus(cancelledDownloadJobState, DownloadState.Status.CANCELLED)
        assertEquals(DownloadState.Status.CANCELLED, service.getDownloadJobStatus(cancelledDownloadJobState))
        testDispatcher.advanceTimeBy(PROGRESS_UPDATE_INTERVAL)
        // The additional notification is the summary one (the notification group).
        assertEquals(1, shadowNotificationService.size())

        val download = DownloadState("https://example.com/file.txt", "file.txt")
        val downloadIntent = Intent("ACTION_DOWNLOAD")
        downloadIntent.putExtra(EXTRA_DOWNLOAD_ID, download.id)

        // Start another download to ensure its notifications are presented
        browserStore.dispatch(DownloadAction.AddDownloadAction(download)).joinBlocking()
        service.onStartCommand(downloadIntent, 0, 0)
        service.downloadJobs.values.forEach { it.job?.join() }
        verify(service, times(2)).performDownload(providedDownload.capture(), anyBoolean())
        service.downloadJobs[providedDownload.value.state.id]?.job?.join()

        val downloadJobState = service.downloadJobs[providedDownload.value.state.id]!!

        service.setDownloadJobStatus(downloadJobState, DownloadState.Status.COMPLETED)
        assertEquals(DownloadState.Status.COMPLETED, service.getDownloadJobStatus(downloadJobState))
        testDispatcher.advanceTimeBy(PROGRESS_UPDATE_INTERVAL)
        // one of the notifications it is the group notification only for devices the support it
        assertEquals(2, shadowNotificationService.size())
    }

    @Test
    fun `createDirectoryIfNeeded - MUST create directory when it does not exists`() = runBlocking {
        val download = DownloadState(destinationDirectory = Environment.DIRECTORY_DOWNLOADS, url = "")

        val file = File(download.directoryPath)
        file.delete()

        assertFalse(file.exists())

        service.createDirectoryIfNeeded(download)

        assertTrue(file.exists())
    }

    @Test
    fun `keeps track of how many seconds have passed since the last update to a notification`() = runBlocking {
        val downloadJobState = DownloadJobState(state = mock(), status = DOWNLOADING)
        val oneSecond = 1000L

        downloadJobState.lastNotificationUpdate = System.currentTimeMillis()

        delay(oneSecond)

        var seconds = downloadJobState.getSecondsSinceTheLastNotificationUpdate()

        assertEquals(1, seconds)

        delay(oneSecond)

        seconds = downloadJobState.getSecondsSinceTheLastNotificationUpdate()

        assertEquals(2, seconds)
    }

    @Test
    fun `is a notification under the time limit for updates`() = runBlocking {
        val downloadJobState = DownloadJobState(state = mock(), status = DOWNLOADING)
        val oneSecond = 1000L

        downloadJobState.lastNotificationUpdate = System.currentTimeMillis()

        assertFalse(downloadJobState.isUnderNotificationUpdateLimit())

        delay(oneSecond)

        assertTrue(downloadJobState.isUnderNotificationUpdateLimit())
    }

    @Test
    fun `try to update a notification`() = runBlocking {
        val downloadJobState = DownloadJobState(state = mock(), status = DOWNLOADING)
        val oneSecond = 1000L

        downloadJobState.lastNotificationUpdate = System.currentTimeMillis()

        // It's over the notification limit
        assertFalse(downloadJobState.canUpdateNotification())

        delay(oneSecond)

        // It's under the notification limit
        assertTrue(downloadJobState.canUpdateNotification())

        downloadJobState.notifiedStopped = true

        assertFalse(downloadJobState.canUpdateNotification())

        downloadJobState.notifiedStopped = false

        assertTrue(downloadJobState.canUpdateNotification())
    }

    @Test
    fun `copyInChunks must alter download currentBytesCopied`() = runBlocking {
        val downloadJobState = DownloadJobState(state = mock(), status = DOWNLOADING)
        val inputStream = mock<InputStream>()

        assertEquals(0, downloadJobState.currentBytesCopied)

        doReturn(15, -1).`when`(inputStream).read(any())
        doNothing().`when`(service).updateDownloadState(any())

        service.copyInChunks(downloadJobState, inputStream, mock())

        assertEquals(15, downloadJobState.currentBytesCopied)
    }

    @Test
    fun `copyInChunks - must return ERROR_IN_STREAM_CLOSED when inStream is closed`() = runBlocking {
        val downloadJobState = DownloadJobState(state = mock(), status = DOWNLOADING)
        val inputStream = mock<InputStream>()

        assertEquals(0, downloadJobState.currentBytesCopied)

        doAnswer { throw IOException() }.`when`(inputStream).read(any())
        doNothing().`when`(service).updateDownloadState(any())
        doNothing().`when`(service).performDownload(any(), anyBoolean())

        val status = service.copyInChunks(downloadJobState, inputStream, mock())

        verify(service).performDownload(downloadJobState, true)
        assertEquals(ERROR_IN_STREAM_CLOSED, status)
    }

    @Test
    fun `copyInChunks - must throw when inStream is closed and download was performed using http client`() = runBlocking {
        val downloadJobState = DownloadJobState(state = mock(), status = DOWNLOADING)
        val inputStream = mock<InputStream>()
        var exceptionWasThrown = false

        assertEquals(0, downloadJobState.currentBytesCopied)

        doAnswer { throw IOException() }.`when`(inputStream).read(any())
        doNothing().`when`(service).updateDownloadState(any())
        doNothing().`when`(service).performDownload(any(), anyBoolean())

        try {
            service.copyInChunks(downloadJobState, inputStream, mock(), true)
        } catch (e: IOException) {
            exceptionWasThrown = true
        }

        verify(service, times(0)).performDownload(downloadJobState, true)
        assertTrue(exceptionWasThrown)
    }

    @Test
    fun `copyInChunks - must return COMPLETED when finish copying bytes`() = runBlocking {
        val downloadJobState = DownloadJobState(state = mock(), status = DOWNLOADING)
        val inputStream = mock<InputStream>()

        assertEquals(0, downloadJobState.currentBytesCopied)

        doReturn(15, -1).`when`(inputStream).read(any())
        doNothing().`when`(service).updateDownloadState(any())

        val status = service.copyInChunks(downloadJobState, inputStream, mock())

        verify(service, never()).performDownload(any(), anyBoolean())

        assertEquals(15, downloadJobState.currentBytesCopied)
        assertEquals(AbstractFetchDownloadService.CopyInChuckStatus.COMPLETED, status)
    }

    @Test
    fun `getSafeContentType - WHEN the file content type is available THEN use it`() {
        val contentTypeFromFile = "application/pdf; qs=0.001"
        val spyContext = spy(testContext)
        val contentResolver = mock<ContentResolver>()

        doReturn(contentTypeFromFile).`when`(contentResolver).getType(any())
        doReturn(contentResolver).`when`(spyContext).contentResolver

        val result = AbstractFetchDownloadService.getSafeContentType(spyContext, mock<Uri>(), "any")

        assertEquals("application/pdf", result)
    }

    @Test
    fun `getSafeContentType - WHEN the file content type is not available THEN use the provided content type`() {
        val contentType = " application/pdf "
        val spyContext = spy(testContext)
        val contentResolver = mock<ContentResolver>()
        doReturn(contentResolver).`when`(spyContext).contentResolver

        doReturn(null).`when`(contentResolver).getType(any())
        var result = AbstractFetchDownloadService.getSafeContentType(spyContext, mock<Uri>(), contentType)
        assertEquals("application/pdf", result)

        doReturn("").`when`(contentResolver).getType(any())
        result = AbstractFetchDownloadService.getSafeContentType(spyContext, mock<Uri>(), contentType)
        assertEquals("application/pdf", result)
    }

    @Test
    fun `getSafeContentType - WHEN none of the provided content types are available THEN return a generic content type`() {
        val spyContext = spy(testContext)
        val contentResolver = mock<ContentResolver>()
        doReturn(contentResolver).`when`(spyContext).contentResolver

        doReturn(null).`when`(contentResolver).getType(any())
        var result = AbstractFetchDownloadService.getSafeContentType(spyContext, mock<Uri>(), null)
        assertEquals("*/*", result)

        doReturn("").`when`(contentResolver).getType(any())
        result = AbstractFetchDownloadService.getSafeContentType(spyContext, mock<Uri>(), null)
        assertEquals("*/*", result)
    }

    // Following 3 tests use the String version of #getSafeContentType while the above 3 tested the Uri version
    // The String version just overloads and delegates the Uri one but being in a companion object we cannot
    // verify the delegation so we are left to verify the result to prevent any regressions.
    @Test
    @Config(shadows = [ShadowFileProvider::class])
    fun `getSafeContentType2 - WHEN the file content type is available THEN use it`() {
        val contentTypeFromFile = "application/pdf; qs=0.001"
        val spyContext = spy(testContext)
        val contentResolver = mock<ContentResolver>()

        doReturn(contentTypeFromFile).`when`(contentResolver).getType(any())
        doReturn(contentResolver).`when`(spyContext).contentResolver

        val result = AbstractFetchDownloadService.getSafeContentType(spyContext, "any", "any")

        assertEquals("application/pdf", result)
    }

    @Test
    @Config(shadows = [ShadowFileProvider::class])
    fun `getSafeContentType2 - WHEN the file content type is not available THEN use the provided content type`() {
        val contentType = " application/pdf "
        val spyContext = spy(testContext)
        val contentResolver = mock<ContentResolver>()
        doReturn(contentResolver).`when`(spyContext).contentResolver

        doReturn(null).`when`(contentResolver).getType(any())
        var result = AbstractFetchDownloadService.getSafeContentType(spyContext, "any", contentType)
        assertEquals("application/pdf", result)

        doReturn("").`when`(contentResolver).getType(any())
        result = AbstractFetchDownloadService.getSafeContentType(spyContext, "any", contentType)
        assertEquals("application/pdf", result)
    }

    @Test
    @Config(shadows = [ShadowFileProvider::class])
    fun `getSafeContentType2 - WHEN none of the provided content types are available THEN return a generic content type`() {
        val spyContext = spy(testContext)
        val contentResolver = mock<ContentResolver>()
        doReturn(contentResolver).`when`(spyContext).contentResolver

        doReturn(null).`when`(contentResolver).getType(any())
        var result = AbstractFetchDownloadService.getSafeContentType(spyContext, "any", null)
        assertEquals("*/*", result)

        doReturn("").`when`(contentResolver).getType(any())
        result = AbstractFetchDownloadService.getSafeContentType(spyContext, "any", null)
        assertEquals("*/*", result)
    }

    // Hard to test #getFilePathUri since it only returns the result of a certain Android api call.
    // But let's try.
    @Test
    fun `getFilePathUri - WHEN called without a registered provider THEN exception is thrown`() {
        // There is no app registered provider that could expose a file from the filesystem of the machine running this test.
        // Peeking into the exception would indicate whether the code really called "FileProvider.getUriForFile" as expected.
        var exception: IllegalArgumentException? = null
        try {
            AbstractFetchDownloadService.getFilePathUri(testContext, "test.txt")
        } catch (e: IllegalArgumentException) {
            exception = e
        }

        assertTrue(exception!!.stackTrace[0].fileName.contains("FileProvider"))
        assertTrue(exception.stackTrace[0].methodName == "getUriForFile")
    }

    @Test
    @Config(shadows = [ShadowFileProvider::class])
    fun `getFilePathUri - WHEN called THEN return a file provider path for the filePath`() {
        // Test that the String filePath is passed to the provider from which we expect a Uri path
        val result = AbstractFetchDownloadService.getFilePathUri(testContext, "location/test.txt")

        assertTrue(result.toString().endsWith("location/test.txt"))
    }
}

@Implements(FileProvider::class)
object ShadowFileProvider {
    @Implementation
    @JvmStatic
    @Suppress("UNUSED_PARAMETER")
    fun getUriForFile(
        context: Context?,
        authority: String?,
        file: File
    ) = "content://authority/random/location/${file.name}".toUri()
}
