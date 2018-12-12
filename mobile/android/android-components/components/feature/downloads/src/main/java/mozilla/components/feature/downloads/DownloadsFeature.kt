/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import android.Manifest.permission.INTERNET
import android.Manifest.permission.WRITE_EXTERNAL_STORAGE
import android.annotation.SuppressLint
import android.content.Context
import android.support.v4.app.FragmentManager
import mozilla.components.browser.session.Download
import mozilla.components.browser.session.SelectionAwareSessionObserver
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.downloads.DownloadDialogFragment.Companion.FRAGMENT_TAG
import mozilla.components.support.base.feature.LifecycleAwareFeature
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
 * @property downloadManager a reference of [DownloadManager] who is in charge of performing the downloads.
 * @property sessionManager a reference of [SessionManager].
 * @property fragmentManager a reference of [FragmentManager]. If you provide a [fragmentManager]
 * a dialog will show before every download.
 * @property fragmentManager a reference of [FragmentManager].
 * @property dialog a reference of [DownloadDialogFragment]. If it is not provided, it will be
 * initialized with an instance of [SimpleDownloadDialogFragment].
 */
class DownloadsFeature(
    private val applicationContext: Context,
    var onNeedToRequestPermissions: OnNeedToRequestPermissions = { _, _ -> },
    var onDownloadCompleted: OnDownloadCompleted = { _, _ -> },
    private val downloadManager: DownloadManager = DownloadManager(applicationContext, onDownloadCompleted),
    private val sessionManager: SessionManager,
    private val fragmentManager: FragmentManager? = null,
    private var dialog: DownloadDialogFragment = SimpleDownloadDialogFragment.newInstance()
) : SelectionAwareSessionObserver(sessionManager), LifecycleAwareFeature {

    /**
     * Starts observing any download on the selected session and send it to the [DownloadManager]
     * to be processed.
     */
    override fun start() {
        super.observeSelected()

        findPreviousDialogFragment()?.let {
            reAttachOnStartDownloadListener(it)
        }
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

            if (fragmentManager != null) {
                showDialog(download, session)
                false
            } else {
                downloadManager.download(download)
                true
            }
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

    @SuppressLint("MissingPermission")
    private fun showDialog(download: Download, session: Session) {
        dialog.setDownload(download)

        dialog.onStartDownload = {
            downloadManager.download(download)
            session.download.consume { true }
        }

        if (!isAlreadyADialogCreated()) {
            dialog.show(fragmentManager, FRAGMENT_TAG)
        }
    }

    private fun isAlreadyADialogCreated(): Boolean {
        return findPreviousDialogFragment() != null
    }

    private fun reAttachOnStartDownloadListener(previousDialog: DownloadDialogFragment?) {
        previousDialog?.apply {
            this@DownloadsFeature.dialog = this
            val selectedSession = sessionManager.selectedSession
            selectedSession?.download?.consume {
                onDownload(selectedSession, it)
                false
            }
        }
    }

    private fun findPreviousDialogFragment(): DownloadDialogFragment? {
        return fragmentManager?.findFragmentByTag(FRAGMENT_TAG) as? DownloadDialogFragment
    }
}
