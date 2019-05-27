/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import android.Manifest.permission.INTERNET
import android.Manifest.permission.WRITE_EXTERNAL_STORAGE
import android.annotation.SuppressLint
import android.content.Context
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PRIVATE
import androidx.fragment.app.FragmentManager
import mozilla.components.browser.session.Download
import mozilla.components.browser.session.SelectionAwareSessionObserver
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.downloads.DownloadDialogFragment.Companion.FRAGMENT_TAG
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.base.observer.Consumable
import mozilla.components.support.ktx.android.content.isPermissionGranted

typealias OnNeedToRequestPermissions = (permissions: Array<String>) -> Unit

/**
 * Feature implementation to provide download functionality for the selected
 * session. The feature will subscribe to the selected session and listen
 * for downloads.
 *
 * @property applicationContext a reference to the application context.
 * @property onNeedToRequestPermissions a callback invoked when permissions
 * need to be requested before a download can be performed. Once the request
 * is completed, [onPermissionsResult] needs to be invoked.
 * @property onDownloadCompleted a callback invoked when a download is completed.
 * @property downloadManager a reference to the [DownloadManager] which is
 * responsible for performing the downloads.
 * @property sessionManager a reference to the application's [SessionManager].
 * @property fragmentManager a reference to a [FragmentManager]. If a fragment
 * manager is provided, a dialog will be shown before every download.
 * @property dialog a reference to a [DownloadDialogFragment]. If not provided, an
 * instance of [SimpleDownloadDialogFragment] will be used.
 */
class DownloadsFeature(
    private val applicationContext: Context,
    var onNeedToRequestPermissions: OnNeedToRequestPermissions = { },
    var onDownloadCompleted: OnDownloadCompleted = { _, _ -> },
    private val downloadManager: DownloadManager = DownloadManager(applicationContext, onDownloadCompleted),
    sessionManager: SessionManager,
    private val sessionId: String? = null,
    private val fragmentManager: FragmentManager? = null,
    @VisibleForTesting(otherwise = PRIVATE)
    internal var dialog: DownloadDialogFragment = SimpleDownloadDialogFragment.newInstance()
) : SelectionAwareSessionObserver(sessionManager), LifecycleAwareFeature {

    /**
     * Starts observing downloads on the selected session and sends them to the [DownloadManager]
     * to be processed.
     */
    override fun start() {
        observeIdOrSelected(sessionId)

        findPreviousDialogFragment()?.let {
            reAttachOnStartDownloadListener(it)
        }
    }

    /**
     * Stops observing downloads on the selected session.
     */
    override fun stop() {
        super.stop()
        downloadManager.unregisterListener()
    }

    /**
     * Notifies the [DownloadManager] that a new download must be processed.
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
            onNeedToRequestPermissions(arrayOf(INTERNET, WRITE_EXTERNAL_STORAGE))
            false
        }
    }

    /**
     * Notifies the feature that the permissions request was completed. It will then
     * either trigger or clear the pending download.
     */
    fun onPermissionsResult(permissions: Array<String>, grantResults: IntArray) {
        if (applicationContext.isPermissionGranted(INTERNET, WRITE_EXTERNAL_STORAGE)) {
            activeSession?.let { session ->
                session.download.consume {
                    onDownload(session, it)
                }
            }
        } else {
            activeSession?.download = Consumable.empty()
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
            activeSession?.let { session ->
                session.download.consume {
                    onDownload(session, it)
                    false
                }
            }
        }
    }

    private fun findPreviousDialogFragment(): DownloadDialogFragment? {
        return fragmentManager?.findFragmentByTag(FRAGMENT_TAG) as? DownloadDialogFragment
    }
}
