/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import android.Manifest.permission.INTERNET
import android.Manifest.permission.WRITE_EXTERNAL_STORAGE
import android.annotation.SuppressLint
import android.content.Context
import mozilla.components.browser.session.Download
import mozilla.components.browser.session.SelectionAwareSessionObserver
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.support.ktx.android.content.isPermissionGranted

typealias OnNeedToRequestPermissions = (session: Session, download: Download) -> Unit

/**
 * Feature implementation for proving download functionality for the selected session.
 * It will subscribe to the selected session and will listening for downloads.
 *
 * @property applicationContext a reference to [Context] applicationContext.
 * @property onNeedToRequestPermissions it will be call when you need to request permission,
 * before a download can be performed.
 * @property onDownloadCompleted a callback to be notified when a download is completed.
 * @property downloadManager a reference of the [DownloadManager] who is in charge of performing the downloads.
 * @property sessionManager a reference of the [SessionManager].
 */
class DownloadsFeature(
    private val applicationContext: Context,
    var onNeedToRequestPermissions: OnNeedToRequestPermissions = { _, _ -> },
    var onDownloadCompleted: OnDownloadCompleted = { _, _ -> },
    private val downloadManager: DownloadManager = DownloadManager(applicationContext, onDownloadCompleted),
    private val sessionManager: SessionManager
) : SelectionAwareSessionObserver(sessionManager) {

    /**
     * Starts observing any download on the selected session and send it to the [DownloadManager]
     * to be processed.
     */
    fun start() {
        super.observeSelected()
    }

    /**
     * Stops observing any download on the selected session.
     */
    override fun stop() {
        super.stop()
        downloadManager.unregisterListener()
    }

    /**
     * Notifies to the [DownloadManager] that a new download must be processed.
     */
    @SuppressLint("MissingPermission")
    override fun onDownload(session: Session, download: Download): Boolean {
        return if (applicationContext.isPermissionGranted(INTERNET, WRITE_EXTERNAL_STORAGE)) {
            downloadManager.download(download)
            true
        } else {
            onNeedToRequestPermissions(session, download)
            false
        }
    }

    /**
     * Call this method to notify the feature download that the permissions where granted.
     * If so it will automatically trigger the pending download.
     */
    fun onPermissionsGranted() {
        activeSession?.let { session ->
            session.download.consume {
                onDownload(session, it)
            }
        }
    }
}
