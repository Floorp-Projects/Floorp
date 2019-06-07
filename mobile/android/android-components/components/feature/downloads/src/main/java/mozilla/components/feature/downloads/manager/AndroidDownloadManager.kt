/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads.manager

import android.Manifest.permission.INTERNET
import android.Manifest.permission.WRITE_EXTERNAL_STORAGE
import android.app.DownloadManager.ACTION_DOWNLOAD_COMPLETE
import android.app.DownloadManager.EXTRA_DOWNLOAD_ID
import android.app.DownloadManager.Request.VISIBILITY_VISIBLE_NOTIFY_COMPLETED
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.net.Uri
import android.util.LongSparseArray
import android.widget.Toast
import androidx.annotation.RequiresPermission
import androidx.core.content.getSystemService
import androidx.core.net.toUri
import androidx.core.util.contains
import androidx.core.util.isEmpty
import androidx.core.util.set
import mozilla.components.browser.session.Download
import mozilla.components.feature.downloads.R
import mozilla.components.support.ktx.android.content.appName
import mozilla.components.support.ktx.android.content.isPermissionGranted
import mozilla.components.support.utils.DownloadUtils

typealias SystemDownloadManager = android.app.DownloadManager
typealias SystemRequest = android.app.DownloadManager.Request

/**
 * Handles the interactions with the [AndroidDownloadManager].
 * @property onDownloadCompleted a callback to be notified when a download is completed.
 * @property applicationContext a reference to [Context] applicationContext.
 */
class AndroidDownloadManager(
    private val applicationContext: Context,
    override var onDownloadCompleted: OnDownloadCompleted = { _, _ -> }
) : DownloadManager {

    private val queuedDownloads = LongSparseArray<Download>()
    private var isSubscribedReceiver = false
    private lateinit var androidDownloadManager: SystemDownloadManager

    /**
     * Schedule a download through the [AndroidDownloadManager].
     * @param download metadata related to the download.
     * @param refererUrl the url from where this download was referred.
     * @param cookie any additional cookie to add as part of the download request.
     * @return the id reference of the scheduled download.
     */
    @RequiresPermission(allOf = [INTERNET, WRITE_EXTERNAL_STORAGE])
    override fun download(
        download: Download,
        refererUrl: String,
        cookie: String
    ): Long {

        if (download.isSupportedProtocol()) {
            // We are ignoring everything that is not http or https. This is a limitation of
            // Android's download manager. There's no reason to show a download dialog for
            // something we can't download anyways.
            showUnSupportFileErrorMessage()
            return FILE_NOT_SUPPORTED
        }

        if (!applicationContext.isPermissionGranted(INTERNET, WRITE_EXTERNAL_STORAGE)) {
            throw SecurityException("You must be granted INTERNET and WRITE_EXTERNAL_STORAGE permissions")
        }

        androidDownloadManager = applicationContext.getSystemService()!!

        val fileName = getFileName(download)

        val request = SystemRequest(download.url.toUri())
            .setNotificationVisibility(VISIBILITY_VISIBLE_NOTIFY_COMPLETED)

        if (!download.contentType.isNullOrEmpty()) {
            request.setMimeType(download.contentType)
        }

        with(request) {
            addRequestHeaderSafely("User-Agent", download.userAgent)
            addRequestHeaderSafely("Cookie", cookie)
            addRequestHeaderSafely("Referer", refererUrl)
        }

        request.setDestinationInExternalPublicDir(download.destinationDirectory, fileName)

        val downloadID = androidDownloadManager.enqueue(request)
        queuedDownloads[downloadID] = download

        if (!isSubscribedReceiver) {
            registerBroadcastReceiver(applicationContext)
        }

        return downloadID
    }

    /**
     * Remove all the listeners.
     */
    override fun unregisterListener() {
        if (isSubscribedReceiver) {
            applicationContext.unregisterReceiver(onDownloadComplete)
            isSubscribedReceiver = false
            queuedDownloads.clear()
        }
    }

    private fun registerBroadcastReceiver(context: Context) {
        val filter = IntentFilter(ACTION_DOWNLOAD_COMPLETE)
        context.registerReceiver(onDownloadComplete, filter)
        isSubscribedReceiver = true
    }

    private fun getFileName(download: Download): String? {
        return if (download.fileName.isNotEmpty()) {
            download.fileName
        } else {
            DownloadUtils.guessFileName(
                "",
                download.url,
                download.contentType
            )
        }
    }

    private val onDownloadComplete: BroadcastReceiver = object : BroadcastReceiver() {

        override fun onReceive(context: Context, intent: Intent) {
            if (queuedDownloads.isEmpty()) {
                unregisterListener()
            }

            val downloadID = intent.getLongExtra(EXTRA_DOWNLOAD_ID, -1)

            if (downloadID in queuedDownloads) {
                val download = queuedDownloads[downloadID]

                download?.let {
                    onDownloadCompleted.invoke(download, downloadID)
                }
                queuedDownloads.remove(downloadID)

                if (queuedDownloads.isEmpty()) {
                    unregisterListener()
                }
            }
        }
    }

    private fun Download.isSupportedProtocol(): Boolean {
        val scheme = Uri.parse(url.trim()).scheme
        return (scheme == null || scheme != "http" && scheme != "https")
    }

    private fun showUnSupportFileErrorMessage() {
        val text = applicationContext.getString(
            R.string.mozac_feature_downloads_file_not_supported2,
            applicationContext.appName)

        Toast.makeText(applicationContext, text, Toast.LENGTH_LONG)
            .show()
    }
}

internal fun SystemRequest.addRequestHeaderSafely(name: String, value: String?) {
    if (value.isNullOrEmpty()) return
    addRequestHeader(name, value)
}
