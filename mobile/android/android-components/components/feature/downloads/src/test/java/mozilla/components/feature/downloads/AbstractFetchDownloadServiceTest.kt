/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import android.app.DownloadManager
import android.app.DownloadManager.EXTRA_DOWNLOAD_ID
import android.app.Notification
import android.app.NotificationManager
import android.app.Service
import android.content.Context
import android.content.Intent
import android.os.Build
import androidx.core.app.NotificationManagerCompat
import androidx.core.content.getSystemService
import androidx.localbroadcastmanager.content.LocalBroadcastManager
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.setMain
import mozilla.components.browser.state.action.DownloadAction
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.state.content.DownloadState.Status.DOWNLOADING
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.feature.downloads.AbstractFetchDownloadService.Companion.ACTION_CANCEL
import mozilla.components.feature.downloads.AbstractFetchDownloadService.Companion.ACTION_PAUSE
import mozilla.components.feature.downloads.AbstractFetchDownloadService.Companion.ACTION_RESUME
import mozilla.components.feature.downloads.AbstractFetchDownloadService.Companion.ACTION_TRY_AGAIN
import mozilla.components.feature.downloads.AbstractFetchDownloadService.Companion.PROGRESS_UPDATE_INTERVAL
import mozilla.components.feature.downloads.AbstractFetchDownloadService.DownloadJobState
import mozilla.components.feature.downloads.DownloadNotification.NOTIFICATION_DOWNLOAD_GROUP_ID
import mozilla.components.feature.downloads.facts.DownloadsFacts.Items.NOTIFICATION
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.processor.CollectionProcessor
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Assert.assertNull
import org.junit.Ignore
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.ArgumentMatchers.anyString
import org.mockito.ArgumentMatchers.anyLong
import org.mockito.ArgumentMatchers.isNull
import org.mockito.Mock
import org.mockito.Mockito.doCallRealMethod
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.doThrow
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.MockitoAnnotations.initMocks
import org.robolectric.Shadows.shadowOf
import org.robolectric.annotation.Config
import org.robolectric.shadows.ShadowNotificationManager
import java.io.IOException
import java.io.InputStream
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
        initMocks(this)
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
        verify(service).performDownload(providedDownload.capture())

        assertEquals(download.url, providedDownload.value.state.url)
        assertEquals(download.fileName, providedDownload.value.state.fileName)

        // Ensure the job is properly added to the map
        assertEquals(1, service.downloadJobs.count())
        assertNotNull(service.downloadJobs[providedDownload.value.state.id])
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
            downloadDeleted = false
        )

        service.verifyDownload(downloadJobState)

        verify(service).setDownloadJobStatus(downloadJobState, DownloadState.Status.FAILED)
        verify(service).updateDownloadState(downloadState.copy(status = DownloadState.Status.FAILED))
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
            foregroundServiceId = 1,
            downloadDeleted = false
        )

        service.verifyDownload(downloadJobState)

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
            foregroundServiceId = 1,
            downloadDeleted = false
        )

        service.verifyDownload(downloadJobState)

        verify(service, times(0)).setDownloadJobStatus(downloadJobState, DownloadState.Status.FAILED)
        verify(service, times(0)).updateDownloadState(downloadState.copy(status = DownloadState.Status.FAILED))
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
            foregroundServiceId = 1,
            downloadDeleted = false
        )

        service.verifyDownload(downloadJobState)

        verify(service, times(0)).setDownloadJobStatus(downloadJobState, DownloadState.Status.FAILED)
        verify(service, times(0)).updateDownloadState(downloadState.copy(status = DownloadState.Status.FAILED))
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
            foregroundServiceId = 1,
            downloadDeleted = false
        )

        service.verifyDownload(downloadJobState)

        verify(service, times(0)).setDownloadJobStatus(downloadJobState, DownloadState.Status.FAILED)
        verify(service, times(0)).updateDownloadState(downloadState.copy(status = DownloadState.Status.FAILED))
    }

    @Test
    fun `verify that a COMPLETED download contains a file size`() {
        val downloadState = DownloadState(url = "mozilla.org/mozilla.txt",
                contentLength = 0L,
                currentBytesCopied = 50,
                status = DOWNLOADING
        )
        val downloadJobState = DownloadJobState(
                job = null,
                state = downloadState,
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
        verify(service).performDownload(providedDownload.capture())

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
        verify(service).performDownload(providedDownload.capture())

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
        verify(service).performDownload(providedDownload.capture())

        // Simulate a pause
        var downloadJobState = service.downloadJobs[providedDownload.value.state.id]!!
        val newState = downloadJobState.state.copy(currentBytesCopied = 1)
        service.downloadJobs[newState.id]?.state = newState
        browserStore.dispatch(DownloadAction.UpdateDownloadAction(newState))

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

    @Ignore("https://github.com/mozilla-mobile/android-components/issues/8042")
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
        verify(service).performDownload(providedDownload.capture())
        service.downloadJobs[providedDownload.value.state.id]?.job?.join()

        // Simulate a failure
        var downloadJobState = service.downloadJobs[providedDownload.value.state.id]!!
        service.setDownloadJobStatus(downloadJobState, DownloadState.Status.FAILED)
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
        verify(service).performDownload(providedDownload.capture())

        service.downloadJobs[providedDownload.value.state.id]?.job?.join()
        val downloadJobState = service.downloadJobs[providedDownload.value.state.id]!!
        assertEquals(DownloadState.Status.FAILED, service.getDownloadJobStatus(downloadJobState))
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
        verify(service).performDownload(providedDownload.capture())

        service.downloadJobs[providedDownload.value.state.id]?.job?.join()
        val downloadJobState = service.downloadJobs[providedDownload.value.state.id]!!
        service.setDownloadJobStatus(downloadJobState, DOWNLOADING)
        assertEquals(DOWNLOADING, service.getDownloadJobStatus(downloadJobState))

        testDispatcher.advanceTimeBy(PROGRESS_UPDATE_INTERVAL)

        // The additional notification is the summary one (the notification group).
        assertEquals(2, shadowNotificationService.size())
    }

    @Test
    fun `onStartCommand sets the notification foreground`() = runBlocking {
        val download = DownloadState("https://example.com/file.txt", "file.txt")

        val downloadIntent = Intent("ACTION_DOWNLOAD")
        downloadIntent.putExtra(EXTRA_DOWNLOAD_ID, download.id)

        doNothing().`when`(service).performDownload(any())

        browserStore.dispatch(DownloadAction.AddDownloadAction(download)).joinBlocking()
        service.onStartCommand(downloadIntent, 0, 0)

        verify(service).setForegroundNotification(any())
    }

    @Test
    fun `sets the notification foreground in devices that support notification group`() = runBlocking {
        val download = DownloadState(id = 1, url = "https://example.com/file.txt", fileName = "file.txt",
                status = DOWNLOADING)
        val downloadState = DownloadJobState(
                state = download,
                foregroundServiceId = Random.nextInt()
        )
        val notification = mock<Notification>()

        doReturn(notification).`when`(service).updateNotificationGroup()

        service.downloadJobs[1L] = downloadState

        service.setForegroundNotification(downloadState)

        verify(service).startForeground(NOTIFICATION_DOWNLOAD_GROUP_ID, notification)
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.M])
    fun `sets the notification foreground in devices that DO NOT support notification group`() {
        val download = DownloadState(id = 1, url = "https://example.com/file.txt",
                fileName = "file.txt", status = DOWNLOADING)
        val downloadState = DownloadJobState(
                state = download,
                foregroundServiceId = Random.nextInt()
        )
        val notification = mock<Notification>()

        doReturn(notification).`when`(service).createCompactForegroundNotification(downloadState)

        service.downloadJobs[1L] = downloadState

        service.setForegroundNotification(downloadState)

        verify(service).startForeground(downloadState.foregroundServiceId, notification)
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.M])
    fun createCompactForegroundNotification() {
        val download = DownloadState(id = 1, url = "https://example.com/file.txt", fileName = "file.txt",
                status = DOWNLOADING)
        val downloadState = DownloadJobState(
                state = download,
                foregroundServiceId = Random.nextInt()
        )

        assertEquals(0, shadowNotificationService.size())

        val notification = service.createCompactForegroundNotification(downloadState)

        service.downloadJobs[1L] = downloadState

        service.setForegroundNotification(downloadState)

        assertNull(notification.group)
        assertEquals(1, shadowNotificationService.size())
        assertNotNull(shadowNotificationService.getNotification(downloadState.foregroundServiceId))
    }

    @Test
    fun `getForegroundId in devices that support notification group will return NOTIFICATION_DOWNLOAD_GROUP_ID`() {
        val download = DownloadState(id = 1, url = "https://example.com/file.txt", fileName = "file.txt")

        val downloadIntent = Intent("ACTION_DOWNLOAD")
        downloadIntent.putExtra(EXTRA_DOWNLOAD_ID, download.id)

        doNothing().`when`(service).performDownload(any())

        service.onStartCommand(downloadIntent, 0, 0)

        assertEquals(NOTIFICATION_DOWNLOAD_GROUP_ID, service.getForegroundId())
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.M])
    fun `getForegroundId in devices that support DO NOT notification group will return the latest active download`() {
        val download = DownloadState(id = 1, url = "https://example.com/file.txt", fileName = "file.txt")

        val downloadIntent = Intent("ACTION_DOWNLOAD")
        downloadIntent.putExtra(EXTRA_DOWNLOAD_ID, download.id)

        doNothing().`when`(service).performDownload(any())

        browserStore.dispatch(DownloadAction.AddDownloadAction(download)).joinBlocking()
        service.onStartCommand(downloadIntent, 0, 0)

        val foregroundId = service.downloadJobs.values.first().foregroundServiceId
        assertEquals(foregroundId, service.getForegroundId())
        assertEquals(foregroundId, service.compatForegroundNotificationId)
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.M])
    fun `updateNotificationGroup will do nothing on devices that do not support notificaiton groups`() = runBlocking {
        val download = DownloadState(id = 1, url = "https://example.com/file.txt", fileName = "file.txt",
                status = DOWNLOADING)
        val downloadState = DownloadJobState(
                state = download,
                foregroundServiceId = Random.nextInt()
        )

        service.downloadJobs[1L] = downloadState

        val notificationGroup = service.updateNotificationGroup()

        assertNull(notificationGroup)
        assertEquals(0, shadowNotificationService.size())
    }

    @Test
    fun `removeDownloadJob will update the background notification if there are other pending downloads`() {
        val download = DownloadState(id = 1, url = "https://example.com/file.txt", fileName = "file.txt",
                status = DOWNLOADING)
        val downloadState = DownloadJobState(
                state = download,
                foregroundServiceId = Random.nextInt()
        )

        service.downloadJobs[1L] = downloadState
        service.downloadJobs[2L] = mock()

        doNothing().`when`(service).updateForegroundNotificationIfNeeded(downloadState)

        service.removeDownloadJob(downloadJobState = downloadState)

        assertEquals(1, service.downloadJobs.size)
        verify(service).updateForegroundNotificationIfNeeded(downloadState)
        verify(service).removeNotification(testContext, downloadState)
    }

    @Test
    fun `removeDownloadJob will stop the service if there are none pending downloads`() {
        val download = DownloadState(id = 1, url = "https://example.com/file.txt", fileName = "file.txt",
                status = DOWNLOADING)
        val downloadState = DownloadJobState(
                state = download,
                foregroundServiceId = Random.nextInt()
        )

        doNothing().`when`(service).stopForeground(false)
        doNothing().`when`(service).clearAllDownloadsNotificationsAndJobs()
        doNothing().`when`(service).stopSelf()

        service.downloadJobs[1L] = downloadState

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
                state = DownloadState(id = 1, url = "https://example.com/file.txt", fileName = "file.txt",
                        status = DownloadState.Status.COMPLETED),
                foregroundServiceId = Random.nextInt()
        )
        val downloadState2 = DownloadJobState(
                state = DownloadState(id = 2, url = "https://example.com/file.txt", fileName = "file.txt",
                        status = DOWNLOADING),
                foregroundServiceId = Random.nextInt()
        )

        service.compatForegroundNotificationId = downloadState1.foregroundServiceId

        service.downloadJobs[1L] = downloadState1
        service.downloadJobs[2L] = downloadState2

        service.updateForegroundNotificationIfNeeded(downloadState1)

        verify(service).setForegroundNotification(downloadState2)
        assertEquals(downloadState2.foregroundServiceId, service.compatForegroundNotificationId)
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.M])
    fun `updateForegroundNotification will NOT select a new foreground notification`() {
        val downloadState1 = DownloadJobState(
            state = DownloadState(id = 1, url = "https://example.com/file.txt", fileName = "file.txt",
                    status = DOWNLOADING),
            foregroundServiceId = Random.nextInt()
        )
        val downloadState2 = DownloadJobState(
            state = DownloadState(id = 2, url = "https://example.com/file.txt", fileName = "file.txt",
                    status = DOWNLOADING),
            foregroundServiceId = Random.nextInt()
        )

        service.compatForegroundNotificationId = downloadState1.foregroundServiceId

        service.downloadJobs[1L] = downloadState1
        service.downloadJobs[2L] = downloadState2

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
        verify(service).performDownload(providedDownload.capture())

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
        verify(service).performDownload(providedDownload.capture())

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
        verify(service).performDownload(providedDownload.capture())

        service.downloadJobs[providedDownload.value.state.id]?.job?.join()
        val downloadJobState = service.downloadJobs[providedDownload.value.state.id]!!
        service.setDownloadJobStatus(downloadJobState, DownloadState.Status.FAILED)
        assertEquals(DownloadState.Status.FAILED, service.getDownloadJobStatus(downloadJobState))

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
        verify(service).performDownload(providedDownload.capture())

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
        verify(service).performDownload(providedDownload.capture())

        val downloadJobState = service.downloadJobs[providedDownload.value.state.id]!!
        assertEquals(DownloadState.Status.FAILED, service.getDownloadJobStatus(downloadJobState))
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
        verify(service).performDownload(providedDownload.capture())

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
        val download = DownloadState("https://example.com/file.txt", "file1.txt",
                status = DOWNLOADING)
        val downloadJob = DownloadJobState(state = mock())
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
        verify(service).performDownload(providedDownload.capture())

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
        val download = DownloadState(id = 1, url = "https://example.com/file.txt", fileName = "file.txt",
                status = DOWNLOADING)
        val downloadState = DownloadJobState(
                state = download,
                foregroundServiceId = Random.nextInt(),
                job = CoroutineScope(IO).launch { while (true) { } }
        )

        service.registerNotificationActionsReceiver()
        service.downloadJobs[download.id] = downloadState

        val notification = DownloadNotification.createOngoingDownloadNotification(testContext, downloadState)

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
    @Config(sdk = [Build.VERSION_CODES.P])
    fun `WHEN a download is completed on devices older than Q the file MUST be added manually to the download system database`() {
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

        val downloadJobState = DownloadJobState(state = download)

        doReturn(testContext).`when`(service).context
        service.updateDownloadNotification(DownloadState.Status.COMPLETED, downloadJobState)

        verify(service).addToDownloadSystemDatabaseCompat(any())
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.P])
    @Suppress("Deprecation")
    fun `do not pass non-http(s) url to addCompletedDownload`() {
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

        service.addToDownloadSystemDatabaseCompat(download)
        verify(downloadManager).addCompletedDownload(anyString(), anyString(), anyBoolean(), anyString(), anyString(), anyLong(), anyBoolean(), isNull(), any())
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.P])
    @Suppress("Deprecation")
    fun `pass http(s) url to addCompletedDownload`() {
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

        service.addToDownloadSystemDatabaseCompat(download)
        verify(downloadManager).addCompletedDownload(anyString(), anyString(), anyBoolean(), anyString(), anyString(), anyLong(), anyBoolean(), any(), any())
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

        verify(service).performDownload(providedDownload.capture())
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
        verify(service, times(2)).performDownload(providedDownload.capture())
        service.downloadJobs[providedDownload.value.state.id]?.job?.join()

        val downloadJobState = service.downloadJobs[providedDownload.value.state.id]!!

        service.setDownloadJobStatus(downloadJobState, DownloadState.Status.COMPLETED)
        assertEquals(DownloadState.Status.COMPLETED, service.getDownloadJobStatus(downloadJobState))
        testDispatcher.advanceTimeBy(PROGRESS_UPDATE_INTERVAL)
        // one of the notifications it is the group notification only for devices the support it
        assertEquals(2, shadowNotificationService.size())
    }

    @Test
    fun `keeps track of how many seconds have passed since the last update to a notification`() = runBlocking {
        val downloadJobState = DownloadJobState(state = mock())
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
        val downloadJobState = DownloadJobState(state = mock())
        val oneSecond = 1000L

        downloadJobState.lastNotificationUpdate = System.currentTimeMillis()

        assertFalse(downloadJobState.isUnderNotificationUpdateLimit())

        delay(oneSecond)

        assertTrue(downloadJobState.isUnderNotificationUpdateLimit())
    }

    @Test
    fun `try to update a notification`() = runBlocking {
        val downloadJobState = DownloadJobState(state = mock())
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
}
