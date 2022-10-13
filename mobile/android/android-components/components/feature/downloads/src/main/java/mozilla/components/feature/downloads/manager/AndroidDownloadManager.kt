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
import android.os.Build
import android.os.Build.VERSION.SDK_INT
import android.util.LongSparseArray
import androidx.annotation.VisibleForTesting
import androidx.core.content.getSystemService
import androidx.core.net.toUri
import androidx.core.util.set
import mozilla.components.browser.state.action.DownloadAction
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.state.content.DownloadState.Status
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.fetch.Headers.Names.COOKIE
import mozilla.components.concept.fetch.Headers.Names.REFERRER
import mozilla.components.concept.fetch.Headers.Names.USER_AGENT
import mozilla.components.feature.downloads.AbstractFetchDownloadService
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
    private val store: BrowserStore,
    override var onDownloadStopped: onDownloadStopped = noop,
) : BroadcastReceiver(), DownloadManager {

    private val downloadRequests = LongSparseArray<SystemRequest>()
    private var isSubscribedReceiver = false

    // Do not require WRITE_EXTERNAL_STORAGE permission on API 29 and above (using scoped storage)
    override val permissions
        get() = if (getSDKVersion() >= Build.VERSION_CODES.Q) {
            arrayOf(INTERNET)
        } else {
            arrayOf(INTERNET, WRITE_EXTERNAL_STORAGE)
        }

    @VisibleForTesting
    internal fun getSDKVersion() = SDK_INT

    /**
     * Schedules a download through the [AndroidDownloadManager].
     * @param download metadata related to the download.
     * @param cookie any additional cookie to add as part of the download request.
     * @return the id reference of the scheduled download.
     */
    override fun download(download: DownloadState, cookie: String): String? {
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
        store.dispatch(DownloadAction.AddDownloadAction(download.copy(id = downloadID.toString())))
        downloadRequests[downloadID] = request
        registerBroadcastReceiver()
        return downloadID.toString()
    }

    override fun tryAgain(downloadId: String) {
        val androidDownloadManager: SystemDownloadManager = applicationContext.getSystemService()!!
        androidDownloadManager.enqueue(downloadRequests[downloadId.toLong()])
    }

    /**
     * Remove all the listeners.
     */
    override fun unregisterListeners() {
        if (isSubscribedReceiver) {
            applicationContext.unregisterReceiver(this)
            isSubscribedReceiver = false
            downloadRequests.clear()
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
     * Invoked when a download is complete. Notifies [onDownloadStopped] and removes the queued
     * download if it's complete.
     */
    override fun onReceive(context: Context, intent: Intent) {
        val downloadID = intent.getStringExtra(EXTRA_DOWNLOAD_ID) ?: ""
        val download = store.state.downloads[downloadID]
        val downloadStatus =
            intent.getSerializableExtra(AbstractFetchDownloadService.EXTRA_DOWNLOAD_STATUS)
                as Status

        if (download != null) {
            onDownloadStopped(download, downloadID, downloadStatus)
        }
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
        DownloadUtils.guessFileName(null, destinationDirectory, url, contentType)
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
