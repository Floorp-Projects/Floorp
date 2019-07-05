/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import android.annotation.TargetApi
import android.app.DownloadManager.ACTION_DOWNLOAD_COMPLETE
import android.app.DownloadManager.EXTRA_DOWNLOAD_ID
import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.content.ContentValues
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.Build.VERSION.SDK_INT
import android.os.Environment
import android.os.IBinder
import android.os.ParcelFileDescriptor
import android.provider.MediaStore
import androidx.core.app.NotificationCompat
import androidx.core.content.getSystemService
import androidx.core.net.toUri
import androidx.localbroadcastmanager.content.LocalBroadcastManager
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.withContext
import mozilla.components.browser.session.Download
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Header
import mozilla.components.concept.fetch.Headers.Names.CONTENT_DISPOSITION
import mozilla.components.concept.fetch.Headers.Names.CONTENT_LENGTH
import mozilla.components.concept.fetch.Headers.Names.CONTENT_TYPE
import mozilla.components.concept.fetch.Headers.Names.REFERRER
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.feature.downloads.ext.addCompletedDownload
import mozilla.components.feature.downloads.ext.getDownloadExtra
import mozilla.components.feature.downloads.manager.getFileName
import mozilla.components.support.base.ids.NotificationIds
import java.io.File
import java.io.FileOutputStream
import java.io.OutputStream

/**
 * Service that performs downloads through a fetch [Client] rather than through the native
 * Android download manager.
 *
 * To use this service, you must create a subclass in your application and it to the manifest.
 *
 * @param broadcastManager Override the [LocalBroadcastManager] instance.
 */
abstract class AbstractFetchDownloadService(
    broadcastManager: LocalBroadcastManager? = null
) : CoroutineService() {

    protected abstract val httpClient: Client
    private val broadcastManager = broadcastManager ?: LocalBroadcastManager.getInstance(this)

    override fun onCreate() {
        startForeground(
            NotificationIds.getIdForTag(this, ONGOING_DOWNLOAD_NOTIFICATION_TAG),
            buildNotification()
        )
        super.onCreate()
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override suspend fun onStartCommand(intent: Intent?, flags: Int) {
        val download = intent?.getDownloadExtra() ?: return
        performDownload(download)

        val downloadID = intent.getLongExtra(EXTRA_DOWNLOAD_ID, -1)
        sendDownloadCompleteBroadcast(downloadID)
    }

    private suspend fun performDownload(download: Download) = withContext(IO) {
        val headers = listOf(
            CONTENT_TYPE to download.contentType,
            CONTENT_LENGTH to download.contentLength?.toString(),
            REFERRER to download.referrerUrl
        ).mapNotNull { (name, value) ->
            if (value.isNullOrBlank()) null else Header(name, value)
        }

        val request = Request(download.url, headers = MutableHeaders(headers))

        val response = httpClient.fetch(request)
        val filename = download.getFileName(response.headers[CONTENT_DISPOSITION])

        response.body.useStream { inStream ->
            useFileStream(download, response, filename) { outStream ->
                inStream.copyTo(outStream)
            }
        }
    }

    /**
     * Informs [mozilla.components.feature.downloads.manager.FetchDownloadManager] that a download
     * has been completed.
     */
    private fun sendDownloadCompleteBroadcast(downloadID: Long) {
        val intent = Intent(ACTION_DOWNLOAD_COMPLETE)
        intent.putExtra(EXTRA_DOWNLOAD_ID, downloadID)
        broadcastManager.sendBroadcast(intent)
    }

    /**
     * Creates an output stream on the local filesystem, then informs the system that a download
     * is complete after [block] is run.
     *
     * Encapsulates different behaviour depending on the SDK version.
     */
    internal fun useFileStream(
        download: Download,
        response: Response,
        filename: String,
        block: (OutputStream) -> Unit
    ) {
        if (SDK_INT >= Build.VERSION_CODES.Q) {
            useFileStreamScopedStorage(download, response, filename, block)
        } else {
            useFileStreamLegacy(download, response, filename, block)
        }
    }

    @Suppress("LongMethod")
    @TargetApi(Build.VERSION_CODES.Q)
    private fun useFileStreamScopedStorage(
        download: Download,
        response: Response,
        filename: String,
        block: (OutputStream) -> Unit
    ) {
        val contentType = response.headers[CONTENT_TYPE]
        val contentLength = response.headers[CONTENT_LENGTH]?.toLongOrNull()

        val values = ContentValues().apply {
            put(MediaStore.Downloads.DISPLAY_NAME, filename)
            put(MediaStore.Downloads.MIME_TYPE, contentType ?: download.contentType ?: "*/*")
            put(MediaStore.Downloads.SIZE, contentLength ?: download.contentLength)
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
    @Suppress("Deprecation", "LongMethod")
    private fun useFileStreamLegacy(
        download: Download,
        response: Response,
        filename: String,
        block: (OutputStream) -> Unit
    ) {
        val dir = Environment.getExternalStoragePublicDirectory(download.destinationDirectory)
        val file = File(dir, filename)
        FileOutputStream(file).use(block)

        val contentType = response.headers[CONTENT_TYPE]
        val contentLength = response.headers[CONTENT_LENGTH]?.toLongOrNull()

        addCompletedDownload(
            title = filename,
            description = filename,
            isMediaScannerScannable = true,
            mimeType = contentType ?: download.contentType ?: "*/*",
            path = file.absolutePath,
            length = contentLength ?: download.contentLength ?: file.length(),
            showNotification = true,
            uri = download.url.toUri(),
            referer = download.referrerUrl?.toUri()
        )
    }

    /**
     * Build the notification to be displayed while the service is active.
     */
    private fun buildNotification(): Notification {
        val channelId = ensureChannelExists(this)

        return NotificationCompat.Builder(this, channelId)
            .setSmallIcon(R.drawable.mozac_feature_download_ic_download)
            .setContentTitle(getString(R.string.mozac_feature_downloads_ongoing_notification_title))
            .setContentText(getString(R.string.mozac_feature_downloads_ongoing_notification_text))
            .setCategory(NotificationCompat.CATEGORY_PROGRESS)
            .setProgress(1, 0, true)
            .setOngoing(true)
            .build()
    }

    /**
     * Make sure a notification channel for download notification exists.
     *
     * Returns the channel id to be used for download notifications.
     */
    private fun ensureChannelExists(context: Context): String {
        if (SDK_INT >= Build.VERSION_CODES.O) {
            val notificationManager: NotificationManager = context.getSystemService()!!

            val channel = NotificationChannel(
                NOTIFICATION_CHANNEL_ID,
                context.getString(R.string.mozac_feature_downloads_notification_channel),
                NotificationManager.IMPORTANCE_DEFAULT
            )

            notificationManager.createNotificationChannel(channel)
        }

        return NOTIFICATION_CHANNEL_ID
    }

    companion object {
        private const val NOTIFICATION_CHANNEL_ID = "Downloads"
        private const val ONGOING_DOWNLOAD_NOTIFICATION_TAG = "OngoingDownload"
    }
}
