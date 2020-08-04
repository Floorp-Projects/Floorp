/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import android.content.Context
import android.widget.Toast
import androidx.annotation.ColorRes
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PRIVATE
import androidx.fragment.app.FragmentManager
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.mapNotNull
import mozilla.components.browser.state.selector.findCustomTabOrSelectedTab
import mozilla.components.browser.state.selector.findTabOrCustomTabOrSelectedTab
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.downloads.DownloadDialogFragment.Companion.FRAGMENT_TAG
import mozilla.components.feature.downloads.manager.AndroidDownloadManager
import mozilla.components.feature.downloads.manager.DownloadManager
import mozilla.components.feature.downloads.manager.noop
import mozilla.components.feature.downloads.manager.onDownloadStopped
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.base.feature.OnNeedToRequestPermissions
import mozilla.components.support.base.feature.PermissionsFeature
import mozilla.components.support.ktx.android.content.appName
import mozilla.components.support.ktx.android.content.isPermissionGranted
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged

/**
 * Feature implementation to provide download functionality for the selected
 * session. The feature will subscribe to the selected session and listen
 * for downloads.
 *
 * @property applicationContext a reference to the application context.
 * @property onNeedToRequestPermissions a callback invoked when permissions
 * need to be requested before a download can be performed. Once the request
 * is completed, [onPermissionsResult] needs to be invoked.
 * @property onDownloadStopped a callback invoked when a download is paused or completed.
 * @property downloadManager a reference to the [DownloadManager] which is
 * responsible for performing the downloads.
 * @property store a reference to the application's [BrowserStore].
 * @property useCases [DownloadsUseCases] instance for consuming processed downloads.
 * @property fragmentManager a reference to a [FragmentManager]. If a fragment
 * manager is provided, a dialog will be shown before every download.
 * @property promptsStyling styling properties for the dialog.
 */
@Suppress("TooManyFunctions", "LongParameterList")
class DownloadsFeature(
    private val applicationContext: Context,
    private val store: BrowserStore,
    private val useCases: DownloadsUseCases,
    override var onNeedToRequestPermissions: OnNeedToRequestPermissions = { },
    onDownloadStopped: onDownloadStopped = noop,
    private val downloadManager: DownloadManager = AndroidDownloadManager(applicationContext, store),
    private val tabId: String? = null,
    private val fragmentManager: FragmentManager? = null,
    private val promptsStyling: PromptsStyling? = null
) : LifecycleAwareFeature, PermissionsFeature {

    var onDownloadStopped: onDownloadStopped
        get() = downloadManager.onDownloadStopped
        set(value) { downloadManager.onDownloadStopped = value }

    init {
        this.onDownloadStopped = onDownloadStopped
    }

    private var scope: CoroutineScope? = null

    /**
     * Starts observing downloads on the selected session and sends them to the [DownloadManager]
     * to be processed.
     */
    @Suppress("Deprecation")
    override fun start() {
        scope = store.flowScoped { flow ->
            flow.mapNotNull { state -> state.findTabOrCustomTabOrSelectedTab(tabId) }
                .ifChanged { it.content.download }
                .collect { state ->
                    state.content.download?.let { downloadState ->
                        processDownload(state, downloadState)
                    }
                }
        }
    }

    /**
     * Calls the tryAgain function of the corresponding [DownloadManager]
     */
    @Suppress("Unused")
    fun tryAgain(id: Long) {
        downloadManager.tryAgain(id)
    }

    /**
     * Stops observing downloads on the selected session.
     */
    override fun stop() {
        scope?.cancel()
        downloadManager.unregisterListeners()
    }

    /**
     * Notifies the [DownloadManager] that a new download must be processed.
     */
    private fun processDownload(tab: SessionState, download: DownloadState): Boolean {
        return if (applicationContext.isPermissionGranted(downloadManager.permissions.asIterable())) {

            if (fragmentManager != null && !download.skipConfirmation) {
                showDialog(tab, download)
                false
            } else {
                useCases.consumeDownload(tab.id, download.id)
                startDownload(download)
            }
        } else {
            onNeedToRequestPermissions(downloadManager.permissions)
            false
        }
    }

    private fun startDownload(download: DownloadState): Boolean {
        val id = downloadManager.download(download)
        return if (id != null) {
            true
        } else {
            showDownloadNotSupportedError()
            false
        }
    }

    /**
     * Notifies the feature that the permissions request was completed. It will then
     * either trigger or clear the pending download.
     */
    override fun onPermissionsResult(permissions: Array<String>, grantResults: IntArray) {
        if (permissions.isEmpty()) {
            // If we are requesting permissions while a permission prompt is already being displayed
            // then Android seems to call `onPermissionsResult` immediately with an empty permissions
            // list. In this case just ignore it.
            return
        }

        withActiveDownload { (tab, download) ->
            if (applicationContext.isPermissionGranted(downloadManager.permissions.asIterable())) {
                processDownload(tab, download)
            } else {
                useCases.consumeDownload(tab.id, download.id)
            }
        }
    }

    @VisibleForTesting(otherwise = PRIVATE)
    internal fun showDownloadNotSupportedError() {
        Toast.makeText(
            applicationContext,
            applicationContext.getString(
                R.string.mozac_feature_downloads_file_not_supported2,
                applicationContext.appName),
            Toast.LENGTH_LONG
        ).show()
    }

    @VisibleForTesting(otherwise = PRIVATE)
    internal fun showDialog(
        tab: SessionState,
        download: DownloadState,
        dialog: DownloadDialogFragment = findPreviousDialogFragment() ?: SimpleDownloadDialogFragment.newInstance(
            promptsStyling = promptsStyling
        )
    ) {
        dialog.setDownload(download)

        dialog.onStartDownload = {
            startDownload(download)
            useCases.consumeDownload.invoke(tab.id, download.id)
        }

        dialog.onCancelDownload = {
            useCases.consumeDownload.invoke(tab.id, download.id)
        }

        if (!isAlreadyADialogCreated() && fragmentManager != null) {
            dialog.showNow(fragmentManager, FRAGMENT_TAG)
        }
    }

    @VisibleForTesting(otherwise = PRIVATE)
    internal fun isAlreadyADialogCreated(): Boolean {
        return findPreviousDialogFragment() != null
    }

    private fun findPreviousDialogFragment(): DownloadDialogFragment? {
        return fragmentManager?.findFragmentByTag(FRAGMENT_TAG) as? DownloadDialogFragment
    }

    private fun withActiveDownload(block: (Pair<SessionState, DownloadState>) -> Unit) {
        val state = store.state.findCustomTabOrSelectedTab(tabId) ?: return
        val download = state.content.download ?: return
        block(Pair(state, download))
    }

    /**
     * Styling for the download dialog prompt
     */
    data class PromptsStyling(
        val gravity: Int,
        val shouldWidthMatchParent: Boolean = false,
        @ColorRes
        val positiveButtonBackgroundColor: Int? = null,
        @ColorRes
        val positiveButtonTextColor: Int? = null,
        val positiveButtonRadius: Float? = null
    )
}
