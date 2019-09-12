/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads.manager

import android.Manifest.permission.FOREGROUND_SERVICE
import android.Manifest.permission.INTERNET
import android.Manifest.permission.WRITE_EXTERNAL_STORAGE
import android.app.DownloadManager.ACTION_DOWNLOAD_COMPLETE
import android.app.DownloadManager.EXTRA_DOWNLOAD_ID
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Build.VERSION.SDK_INT
import android.os.Build.VERSION_CODES.P
import android.util.LongSparseArray
import androidx.core.util.isEmpty
import androidx.core.util.set
import androidx.localbroadcastmanager.content.LocalBroadcastManager
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.feature.downloads.AbstractFetchDownloadService
import mozilla.components.feature.downloads.ext.isScheme
import mozilla.components.feature.downloads.ext.putDownloadExtra
import kotlin.random.Random
import kotlin.reflect.KClass

/**
 * Handles the interactions with [AbstractFetchDownloadService].
 *
 * @property applicationContext a reference to [Context] applicationContext.
 * @property service The subclass of [AbstractFetchDownloadService] to use.
 */
class FetchDownloadManager<T : AbstractFetchDownloadService>(
    private val applicationContext: Context,
    private val service: KClass<T>,
    private val broadcastManager: LocalBroadcastManager = LocalBroadcastManager.getInstance(applicationContext),
    override var onDownloadCompleted: OnDownloadCompleted = noop
) : BroadcastReceiver(), DownloadManager {

    private val queuedDownloads = LongSparseArray<DownloadState>()
    private var isSubscribedReceiver = false

    override val permissions = if (SDK_INT >= P) {
        arrayOf(INTERNET, WRITE_EXTERNAL_STORAGE, FOREGROUND_SERVICE)
    } else {
        arrayOf(INTERNET, WRITE_EXTERNAL_STORAGE)
    }

    /**
     * Schedules a download through the [AbstractFetchDownloadService].
     * @param download metadata related to the download.
     * @param cookie any additional cookie to add as part of the download request.
     * @return the id reference of the scheduled download.
     */
    override fun download(download: DownloadState, cookie: String): Long? {
        if (!download.isScheme(listOf("http", "https"))) {
            // We are ignoring everything that is not http or https. This is a limitation of
            // GeckoView: https://bugzilla.mozilla.org/show_bug.cgi?id=1501735 and
            // https://bugzilla.mozilla.org/show_bug.cgi?id=1432949
            return null
        }

        validatePermissionGranted(applicationContext)

        val downloadID = Random.nextLong()
        queuedDownloads[downloadID] = download

        val intent = Intent(applicationContext, service.java)
        intent.putExtra(EXTRA_DOWNLOAD_ID, downloadID)
        intent.putDownloadExtra(download)
        applicationContext.startService(intent)

        registerBroadcastReceiver()

        return downloadID
    }

    /**
     * Remove all the listeners.
     */
    override fun unregisterListeners() {
        if (isSubscribedReceiver) {
            broadcastManager.unregisterReceiver(this)
            isSubscribedReceiver = false
            queuedDownloads.clear()
        }
    }

    private fun registerBroadcastReceiver() {
        if (!isSubscribedReceiver) {
            val filter = IntentFilter(ACTION_DOWNLOAD_COMPLETE)
            broadcastManager.registerReceiver(this, filter)
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
