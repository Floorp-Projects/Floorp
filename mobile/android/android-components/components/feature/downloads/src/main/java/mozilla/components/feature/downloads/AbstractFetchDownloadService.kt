/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import android.annotation.TargetApi
import android.app.DownloadManager.ACTION_DOWNLOAD_COMPLETE
import android.app.DownloadManager.EXTRA_DOWNLOAD_ID
import android.app.Service
import android.content.BroadcastReceiver
import android.content.ContentValues
import android.content.Context
import android.content.Intent
import android.content.Intent.ACTION_VIEW
import android.content.IntentFilter
import android.os.Build
import android.os.Build.VERSION.SDK_INT
import android.os.Environment
import android.os.IBinder
import android.os.ParcelFileDescriptor
import android.provider.MediaStore
import androidx.annotation.VisibleForTesting
import androidx.core.app.NotificationManagerCompat
import androidx.core.content.FileProvider
import androidx.core.net.toUri
import androidx.localbroadcastmanager.content.LocalBroadcastManager
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Headers.Names.CONTENT_RANGE
import mozilla.components.concept.fetch.Headers.Names.RANGE
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Request
import mozilla.components.feature.downloads.ext.addCompletedDownload
import mozilla.components.feature.downloads.ext.getDownloadExtra
import mozilla.components.feature.downloads.ext.withResponse
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.io.InputStream
import java.io.OutputStream
import kotlin.random.Random

/**
 * Service that performs downloads through a fetch [Client] rather than through the native
 * Android download manager.
 *
 * To use this service, you must create a subclass in your application and it to the manifest.
 */
@Suppress("TooManyFunctions", "LargeClass")
abstract class AbstractFetchDownloadService : Service() {

    protected abstract val httpClient: Client
    @VisibleForTesting
    internal val broadcastManager by lazy { LocalBroadcastManager.getInstance(this) }
    @VisibleForTesting
    internal val context: Context get() = this

    internal var downloadJobs = mutableMapOf<Long, DownloadJobState>()

    internal data class DownloadJobState(
        var job: Job? = null,
        var state: DownloadState,
        var currentBytesCopied: Long = 0,
        var status: DownloadJobStatus,
        var foregroundServiceId: Int = 0
    )

    /**
     * Status of an ongoing download
     */
    enum class DownloadJobStatus {
        ACTIVE,
        PAUSED,
        CANCELLED,
        FAILED,
        COMPLETED
    }

    internal val broadcastReceiver by lazy {
        object : BroadcastReceiver() {
            override fun onReceive(context: Context, intent: Intent?) {
                val downloadId =
                        intent?.extras?.getLong(DownloadNotification.EXTRA_DOWNLOAD_ID) ?: return
                val currentDownloadJobState = downloadJobs[downloadId] ?: return

                when (intent.action) {
                    ACTION_PAUSE -> {
                        currentDownloadJobState.status = DownloadJobStatus.PAUSED
                        currentDownloadJobState.job?.cancel()
                    }

                    ACTION_RESUME -> {
                        currentDownloadJobState.status = DownloadJobStatus.ACTIVE

                        currentDownloadJobState.job = CoroutineScope(IO).launch {
                            startDownloadJob(currentDownloadJobState.state)
                        }
                    }

                    ACTION_CANCEL -> {
                        NotificationManagerCompat.from(context).cancel(
                            currentDownloadJobState.foregroundServiceId
                        )
                        currentDownloadJobState.status = DownloadJobStatus.CANCELLED

                        currentDownloadJobState.job?.cancel()
                    }

                    ACTION_TRY_AGAIN -> {
                        NotificationManagerCompat.from(context).cancel(
                            currentDownloadJobState.foregroundServiceId
                        )
                        currentDownloadJobState.status = DownloadJobStatus.ACTIVE

                        currentDownloadJobState.job = CoroutineScope(IO).launch {
                            startDownloadJob(currentDownloadJobState.state)
                        }
                    }

                    ACTION_OPEN -> {
                        openFile(
                            context = context,
                            filePath = currentDownloadJobState.state.filePath,
                            contentType = currentDownloadJobState.state.contentType
                        )
                    }
                }
            }
        }
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        val download = intent?.getDownloadExtra() ?: return START_REDELIVER_INTENT
        registerForUpdates()

        val foregroundServiceId = Random.nextInt()

        // Create a new job and add it, with its downloadState to the map
        downloadJobs[download.id] = DownloadJobState(
            state = download,
            foregroundServiceId = foregroundServiceId,
            status = DownloadJobStatus.ACTIVE
        )

        downloadJobs[download.id]?.job = CoroutineScope(IO).launch {
            startDownloadJob(download)
        }

        return super.onStartCommand(intent, flags, startId)
    }

    override fun onDestroy() {
        super.onDestroy()
        downloadJobs.values.forEach {
            it.job?.cancel()
        }
    }

    internal fun startDownloadJob(download: DownloadState) {
        val currentDownloadJobState = downloadJobs[download.id] ?: return

        val notification = try {
            performDownload(download)
            when (currentDownloadJobState.status) {
                DownloadJobStatus.PAUSED -> {
                    DownloadNotification.createPausedDownloadNotification(context, download)
                }

                DownloadJobStatus.ACTIVE -> {
                    currentDownloadJobState.status = DownloadJobStatus.COMPLETED
                    DownloadNotification.createDownloadCompletedNotification(context, download)
                }

                DownloadJobStatus.FAILED -> {
                    DownloadNotification.createDownloadFailedNotification(context, download)
                }

                else -> return
            }
        } catch (e: IOException) {
            currentDownloadJobState.status = DownloadJobStatus.FAILED
            DownloadNotification.createDownloadFailedNotification(context, download)
        }

        NotificationManagerCompat.from(context).notify(
            currentDownloadJobState.foregroundServiceId,
            notification
        )

        sendDownloadCompleteBroadcast(download.id, currentDownloadJobState.status)
    }

    private fun registerForUpdates() {
        val filter = IntentFilter().apply {
            addAction(ACTION_PAUSE)
            addAction(ACTION_RESUME)
            addAction(ACTION_CANCEL)
            addAction(ACTION_TRY_AGAIN)
            addAction(ACTION_OPEN)
        }

        context.registerReceiver(broadcastReceiver, filter)
    }

    private fun displayOngoingDownloadNotification(download: DownloadState) {
        val ongoingDownloadNotification = DownloadNotification.createOngoingDownloadNotification(
            context,
            download
        )

        NotificationManagerCompat.from(context).notify(
            downloadJobs[download.id]?.foregroundServiceId ?: 0,
            ongoingDownloadNotification
        )
    }

    @Suppress("ComplexCondition")
    internal fun performDownload(download: DownloadState) {
        val isResumingDownload = downloadJobs[download.id]?.currentBytesCopied ?: 0L > 0L
        val headers = MutableHeaders()

        if (isResumingDownload) {
            headers.append(RANGE, "bytes=${downloadJobs[download.id]?.currentBytesCopied}-")
        }

        val request = Request(download.url, headers = headers)
        val response = httpClient.fetch(request)

        // If we are resuming a download and the response does not contain a CONTENT_RANGE
        // we cannot be sure that the request will properly be handled
        if (response.status != PARTIAL_CONTENT_STATUS && response.status != OK_STATUS ||
            (isResumingDownload && !response.headers.contains(CONTENT_RANGE))) {
            // We experienced a problem trying to fetch the file, send a failure notification
            downloadJobs[download.id]?.currentBytesCopied = 0
            downloadJobs[download.id]?.status = DownloadJobStatus.FAILED
            return
        }

        response.body.useStream { inStream ->
            val newDownloadState = download.withResponse(response.headers, inStream)
            downloadJobs[download.id]?.state = newDownloadState

            displayOngoingDownloadNotification(newDownloadState)

            useFileStream(newDownloadState, isResumingDownload) { outStream ->
                copyInChunks(downloadJobs[download.id]!!, inStream, outStream)
            }
        }
    }

    private fun copyInChunks(downloadJobState: DownloadJobState, inStream: InputStream, outStream: OutputStream) {
        // To ensure that we copy all files (even ones that don't have fileSize, we must NOT check < fileSize
        while (downloadJobState.status == DownloadJobStatus.ACTIVE) {
            val data = ByteArray(CHUNK_SIZE)
            val bytesRead = inStream.read(data)

            // If bytesRead is -1, there's no data left to read from the stream
            if (bytesRead == -1) { break }

            downloadJobState.currentBytesCopied += bytesRead

            outStream.write(data, 0, bytesRead)
        }
    }

    /**
     * Informs [mozilla.components.feature.downloads.manager.FetchDownloadManager] that a download
     * has been completed.
     */
    private fun sendDownloadCompleteBroadcast(downloadID: Long, status: DownloadJobStatus) {
        val intent = Intent(ACTION_DOWNLOAD_COMPLETE)
        intent.putExtra(EXTRA_DOWNLOAD_ID, downloadID)
        intent.putExtra(EXTRA_DOWNLOAD_STATUS, status)
        broadcastManager.sendBroadcast(intent)
    }

    /**
     * Creates an output stream on the local filesystem, then informs the system that a download
     * is complete after [block] is run.
     *
     * Encapsulates different behaviour depending on the SDK version.
     */
    internal fun useFileStream(
        download: DownloadState,
        append: Boolean,
        block: (OutputStream) -> Unit
    ) {
        if (SDK_INT >= Build.VERSION_CODES.Q) {
            useFileStreamScopedStorage(download, block)
        } else {
            useFileStreamLegacy(download, append, block)
        }
    }

    @TargetApi(Build.VERSION_CODES.Q)
    private fun useFileStreamScopedStorage(download: DownloadState, block: (OutputStream) -> Unit) {
        val values = ContentValues().apply {
            put(MediaStore.Downloads.DISPLAY_NAME, download.fileName)
            put(MediaStore.Downloads.MIME_TYPE, download.contentType ?: "*/*")
            put(MediaStore.Downloads.SIZE, download.contentLength)
            put(MediaStore.Downloads.IS_PENDING, 1)
        }

        val resolver = applicationContext.contentResolver
        val collection = MediaStore.Downloads.getContentUri(MediaStore.VOLUME_EXTERNAL_PRIMARY)
        val item = resolver.insert(collection, values)

        val pfd = resolver.openFileDescriptor(item!!, "w")
        ParcelFileDescriptor.AutoCloseOutputStream(pfd).use(block)

        values.clear()
        values.put(MediaStore.Downloads.IS_PENDING, 0)
        resolver.update(item, values, null, null)
    }

    @TargetApi(Build.VERSION_CODES.P)
    @Suppress("Deprecation")
    private fun useFileStreamLegacy(download: DownloadState, append: Boolean, block: (OutputStream) -> Unit) {
        val dir = Environment.getExternalStoragePublicDirectory(download.destinationDirectory)
        val file = File(dir, download.fileName!!)

        FileOutputStream(file, append).use(block)

        addCompletedDownload(
            title = download.fileName!!,
            description = download.fileName!!,
            isMediaScannerScannable = true,
            mimeType = download.contentType ?: "*/*",
            path = file.absolutePath,
            length = download.contentLength ?: file.length(),
            // Only show notifications if our channel is blocked
            showNotification = !DownloadNotification.isChannelEnabled(context),
            uri = download.url.toUri(),
            referer = download.referrerUrl?.toUri()
        )
    }

    companion object {
        /**
         * Launches an intent to open the given file
         */
        fun openFile(context: Context, filePath: String, contentType: String?) {
            // Create a new file with the location of the saved file to extract the correct path
            // `file` has the wrong path, so we must construct it based on the `fileName` and `dir.path`s
            val fileLocation = File(filePath)
            val constructedFilePath = FileProvider.getUriForFile(
                context,
                context.packageName + FILE_PROVIDER_EXTENSION,
                fileLocation
            )

            val newIntent = Intent(ACTION_VIEW).apply {
                setDataAndType(constructedFilePath, contentType ?: "*/*")
                flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_GRANT_READ_URI_PERMISSION
            }

            context.startActivity(newIntent)
        }

        private const val FILE_PROVIDER_EXTENSION = ".fileprovider"
        private const val CHUNK_SIZE = 4 * 1024
        private const val PARTIAL_CONTENT_STATUS = 206
        private const val OK_STATUS = 200

        const val EXTRA_DOWNLOAD_STATUS = "mozilla.components.feature.downloads.extras.DOWNLOAD_STATUS"
        const val ACTION_OPEN = "mozilla.components.feature.downloads.OPEN"
        const val ACTION_PAUSE = "mozilla.components.feature.downloads.PAUSE"
        const val ACTION_RESUME = "mozilla.components.feature.downloads.RESUME"
        const val ACTION_CANCEL = "mozilla.components.feature.downloads.CANCEL"
        const val ACTION_TRY_AGAIN = "mozilla.components.feature.downloads.TRY_AGAIN"
    }
}
