/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import android.annotation.SuppressLint
import android.content.Context
import android.widget.Toast
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PRIVATE
import androidx.fragment.app.FragmentManager
import mozilla.components.browser.session.Download
import mozilla.components.browser.session.SelectionAwareSessionObserver
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.downloads.DownloadDialogFragment.Companion.FRAGMENT_TAG
import mozilla.components.feature.downloads.manager.AndroidDownloadManager
import mozilla.components.feature.downloads.manager.DownloadManager
import mozilla.components.feature.downloads.manager.OnDownloadCompleted
import mozilla.components.feature.downloads.manager.noop
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.base.feature.OnNeedToRequestPermissions
import mozilla.components.support.base.feature.PermissionsFeature
import mozilla.components.support.base.observer.Consumable
import mozilla.components.support.ktx.android.content.appName
import mozilla.components.support.ktx.android.content.isPermissionGranted

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
    override var onNeedToRequestPermissions: OnNeedToRequestPermissions = { },
    onDownloadCompleted: OnDownloadCompleted = noop,
    private val downloadManager: DownloadManager = AndroidDownloadManager(applicationContext),
    sessionManager: SessionManager,
    private val sessionId: String? = null,
    private val fragmentManager: FragmentManager? = null,
    @VisibleForTesting(otherwise = PRIVATE)
    internal var dialog: DownloadDialogFragment = SimpleDownloadDialogFragment.newInstance()
) : SelectionAwareSessionObserver(sessionManager), LifecycleAwareFeature, PermissionsFeature {

    var onDownloadCompleted: OnDownloadCompleted
        get() = downloadManager.onDownloadCompleted
        set(value) { downloadManager.onDownloadCompleted = value }

    init {
        this.onDownloadCompleted = onDownloadCompleted
    }

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
        downloadManager.unregisterListeners()
    }

    /**
     * Notifies the [DownloadManager] that a new download must be processed.
     */
    @SuppressLint("MissingPermission")
    override fun onDownload(session: Session, download: Download): Boolean {
        return if (applicationContext.isPermissionGranted(downloadManager.permissions.asIterable())) {
            if (fragmentManager != null) {
                showDialog(download, session)
                false
            } else {
                startDownload(download)
            }
        } else {
            onNeedToRequestPermissions(downloadManager.permissions)
            false
        }
    }

    private fun startDownload(download: Download): Boolean {
        val id = downloadManager.download(download)
        return if (id != null) {
            true
        } else {
            showUnSupportFileErrorMessage()
            false
        }
    }

    /**
     * Notifies the feature that the permissions request was completed. It will then
     * either trigger or clear the pending download.
     */
    override fun onPermissionsResult(permissions: Array<String>, grantResults: IntArray) {
        if (applicationContext.isPermissionGranted(downloadManager.permissions.asIterable())) {
            activeSession?.let { session ->
                session.download.consume { onDownload(session, it) }
            }
        } else {
            activeSession?.download = Consumable.empty()
        }
    }

    private fun showUnSupportFileErrorMessage() {
        val text = applicationContext.getString(
            R.string.mozac_feature_downloads_file_not_supported2,
            applicationContext.appName
        )

        Toast.makeText(applicationContext, text, Toast.LENGTH_LONG).show()
    }

    @SuppressLint("MissingPermission")
    private fun showDialog(download: Download, session: Session) {
        dialog.setDownload(download)

        dialog.onStartDownload = {
            session.download.consume(this::startDownload)
        }

        if (!isAlreadyADialogCreated() && fragmentManager != null) {
            dialog.show(fragmentManager, FRAGMENT_TAG)
        }
    }

    private fun isAlreadyADialogCreated(): Boolean {
        return findPreviousDialogFragment() != null
    }

    private fun reAttachOnStartDownloadListener(previousDialog: DownloadDialogFragment?) {
        previousDialog?.let {
            dialog = it
            activeSession?.let { session ->
                session.download.consume { download -> onDownload(session, download) }
            }
        }
    }

    private fun findPreviousDialogFragment(): DownloadDialogFragment? {
        return fragmentManager?.findFragmentByTag(FRAGMENT_TAG) as? DownloadDialogFragment
    }
}
