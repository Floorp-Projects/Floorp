/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share

import android.content.Context
import android.content.Intent
import android.content.Intent.ACTION_SEND
import android.content.Intent.EXTRA_TEXT
import android.content.Intent.FLAG_ACTIVITY_NEW_TASK
import androidx.annotation.VisibleForTesting
import androidx.navigation.NavController
import com.google.android.material.snackbar.Snackbar
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.TabData
import mozilla.components.feature.sendtab.SendTabUseCases
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.FenixSnackbarPresenter
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.getRootView
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.share.listadapters.AppShareOption

/**
 * [ShareFragment] controller.
 *
 * Delegated by View Interactors, handles container business logic and operates changes on it.
 */
interface ShareController {
    fun handleReauth()
    fun handleShareClosed()
    fun handleShareToApp(app: AppShareOption)
    fun handleAddNewDevice()
    fun handleShareToDevice(device: Device)
    fun handleShareToAllDevices(devices: List<Device>)
    fun handleSignIn()
}

/**
 * Default behavior of [ShareController]. Other implementations are possible.
 *
 * @param context [Context] used for various Android interactions.
 * @param sharedTabs the list of [ShareTab]s that can be shared.
 * @param sendTabUseCases instance of [SendTabUseCases] which allows sending tabs to account devices.
 * @param snackbarPresenter - instance of [FenixSnackbarPresenter] for displaying styled snackbars
 * @param navController - [NavController] used for navigation.
 * @param dismiss - callback signalling sharing can be closed.
 */
@Suppress("TooManyFunctions")
class DefaultShareController(
    private val context: Context,
    private val sharedTabs: List<ShareTab>,
    private val sendTabUseCases: SendTabUseCases,
    private val snackbarPresenter: FenixSnackbarPresenter,
    private val navController: NavController,
    private val dismiss: () -> Unit
) : ShareController {
    override fun handleReauth() {
        val directions = ShareFragmentDirections.actionShareFragmentToAccountProblemFragment()
        navController.nav(R.id.shareFragment, directions)
        dismiss()
    }

    override fun handleShareClosed() {
        dismiss()
    }

    override fun handleShareToApp(app: AppShareOption) {
        val intent = Intent(ACTION_SEND).apply {
            putExtra(EXTRA_TEXT, getShareText())
            type = "text/plain"
            flags = FLAG_ACTIVITY_NEW_TASK
            setClassName(app.packageName, app.activityName)
        }

        try {
            context.startActivity(intent)
        } catch (e: SecurityException) {
            context.getRootView()?.let {
                FenixSnackbar.make(it, Snackbar.LENGTH_LONG)
                    .setText(context.getString(R.string.share_error_snackbar))
                    .show()
            }
        }
        dismiss()
    }

    override fun handleAddNewDevice() {
        val directions = ShareFragmentDirections.actionShareFragmentToAddNewDeviceFragment()
        navController.navigate(directions)
    }

    override fun handleShareToDevice(device: Device) {
        context.metrics.track(Event.SendTab)
        shareToDevicesWithRetry { sendTabUseCases.sendToDeviceAsync(device.id, sharedTabs.toTabData()) }
    }

    override fun handleShareToAllDevices(devices: List<Device>) {
        shareToDevicesWithRetry { sendTabUseCases.sendToAllAsync(sharedTabs.toTabData()) }
    }

    override fun handleSignIn() {
        context.metrics.track(Event.SignInToSendTab)
        val directions = ShareFragmentDirections.actionShareFragmentToTurnOnSyncFragment()
        navController.nav(R.id.shareFragment, directions)
        dismiss()
    }

    private fun shareToDevicesWithRetry(shareOperation: () -> Deferred<Boolean>) {
        // Use GlobalScope to allow the continuation of this method even if the share fragment is closed.
        GlobalScope.launch(Dispatchers.Main) {
            when (shareOperation.invoke().await()) {
                true -> showSuccess()
                false -> showFailureWithRetryOption { shareToDevicesWithRetry(shareOperation) }
            }
            dismiss()
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    fun showSuccess() {
        snackbarPresenter.present(
            getSuccessMessage(),
            Snackbar.LENGTH_SHORT
        )
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    fun showFailureWithRetryOption(operation: () -> Unit) {
        snackbarPresenter.present(
            text = context.getString(R.string.sync_sent_tab_error_snackbar),
            length = Snackbar.LENGTH_LONG,
            action = operation,
            actionName = context.getString(R.string.sync_sent_tab_error_snackbar_action),
            isError = true
        )
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    fun getSuccessMessage(): String = with(context) {
        when (sharedTabs.size) {
            1 -> getString(R.string.sync_sent_tab_snackbar)
            else -> getString(R.string.sync_sent_tabs_snackbar)
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    fun getShareText() = sharedTabs.joinToString("\n") { tab -> tab.url }

    // Navigation between app fragments uses ShareTab as arguments. SendTabUseCases uses TabData.
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    fun ShareTab.toTabData() = TabData(title, url)

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    fun List<ShareTab>.toTabData() = map { it.toTabData() }
}
