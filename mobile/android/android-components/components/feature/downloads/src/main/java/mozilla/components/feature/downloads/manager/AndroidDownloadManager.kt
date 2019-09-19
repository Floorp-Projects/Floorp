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
import android.util.LongSparseArray
import androidx.annotation.RequiresPermission
import androidx.core.content.getSystemService
import androidx.core.net.toUri
import androidx.core.util.isEmpty
import androidx.core.util.set
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.concept.fetch.Headers.Names.COOKIE
import mozilla.components.concept.fetch.Headers.Names.REFERRER
import mozilla.components.concept.fetch.Headers.Names.USER_AGENT
import mozilla.components.feature.downloads.ext.isScheme
import mozilla.components.support.utils.DownloadUtils

typealias SystemDownloadManager = android.app.DownloadManager
typealias SystemRequest = android.app.DownloadManager.Request

/**
 * Handles the interactions with the [AndroidDownloadManager].
 *
 * @property applicationContext a reference to [Context] applicationContext.
 */
class AndroidDownloadManager(
    private val applicationContext: Context,
    override var onDownloadCompleted: OnDownloadCompleted = noop
) : BroadcastReceiver(), DownloadManager {

    private val queuedDownloads = LongSparseArray<DownloadState>()
    private var isSubscribedReceiver = false

    override val permissions = arrayOf(INTERNET, WRITE_EXTERNAL_STORAGE)

    /**
     * Schedules a download through the [AndroidDownloadManager].
     * @param download metadata related to the download.
     * @param cookie any additional cookie to add as part of the download request.
     * @return the id reference of the scheduled download.
     */
    @RequiresPermission(allOf = [INTERNET, WRITE_EXTERNAL_STORAGE])
    override fun download(download: DownloadState, cookie: String): Long? {

        val androidDownloadManager: SystemDownloadManager = applicationContext.getSystemService()!!

        if (!download.isScheme(listOf("http", "https"))) {
            // We are ignoring everything that is not http or https. This is a limitation of
            // Android's download manager. There's no reason to show a download dialog for
            // something we can't download anyways.
            return null
        }

        validatePermissionGranted(applicationContext)

        val request = download.toAndroidRequest(cookie)

        val downloadID = androidDownloadManager.enqueue(request)
        queuedDownloads[downloadID] = download

        registerBroadcastReceiver()

        return downloadID
    }

    /**
     * Remove all the listeners.
     */
    override fun unregisterListeners() {
        if (isSubscribedReceiver) {
            applicationContext.unregisterReceiver(this)
            isSubscribedReceiver = false
            queuedDownloads.clear()
        }
    }

    private fun registerBroadcastReceiver() {
        if (!isSubscribedReceiver) {
            val filter = IntentFilter(ACTION_DOWNLOAD_COMPLETE)
            applicationContext.registerReceiver(this, filter)
            isSubscribedReceiver = true
        }
    }

    /**
     * Invoked when a download is complete. Calls [onDownloadCompleted] and unregisters the
     * broadcast receiver if there are no more queued downloads.
     */
    override fun onReceive(context: Context, intent: Intent) {
        if (queuedDownloads.isEmpty()) unregisterListeners()

        val downloadID = intent.getLongExtra(EXTRA_DOWNLOAD_ID, -1)
        val download = queuedDownloads[downloadID]

        if (download != null) {
            onDownloadCompleted(download, downloadID)
            queuedDownloads.remove(downloadID)
        }

        if (queuedDownloads.isEmpty()) unregisterListeners()
    }
}

private fun DownloadState.toAndroidRequest(cookie: String): SystemRequest {
    val request = SystemRequest(url.toUri())
        .setNotificationVisibility(VISIBILITY_VISIBLE_NOTIFY_COMPLETED)

    if (!contentType.isNullOrEmpty()) {
        request.setMimeType(contentType)
    }

    with(request) {
        addRequestHeaderSafely(USER_AGENT, userAgent)
        addRequestHeaderSafely(COOKIE, cookie)
        addRequestHeaderSafely(REFERRER, referrerUrl)
    }

    val fileName = if (fileName.isNullOrBlank()) {
        DownloadUtils.guessFileName(null, url, contentType)
    } else {
        fileName
    }
    request.setDestinationInExternalPublicDir(destinationDirectory, fileName)

    return request
}

internal fun SystemRequest.addRequestHeaderSafely(name: String, value: String?) {
    if (value.isNullOrEmpty()) return
    addRequestHeader(name, value)
}
