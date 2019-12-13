/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share

import android.content.Context
import android.content.Intent
import android.content.Intent.ACTION_SEND
import android.content.Intent.EXTRA_TEXT
import android.content.Intent.FLAG_ACTIVITY_NEW_TASK
import android.net.Uri
import androidx.annotation.VisibleForTesting
import androidx.navigation.NavController
import com.google.android.material.snackbar.Snackbar
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.TabData
import mozilla.components.feature.accounts.push.SendTabUseCases
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbarPresenter
import org.mozilla.fenix.components.metrics.Event
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

    enum class Result {
        DISMISSED, SHARE_ERROR, SUCCESS
    }
}

/**
 * Default behavior of [ShareController]. Other implementations are possible.
 *
 * @param context [Context] used for various Android interactions.
 * @param shareData the list of [ShareData]s that can be shared.
 * @param sendTabUseCases instance of [SendTabUseCases] which allows sending tabs to account devices.
 * @param snackbarPresenter - instance of [FenixSnackbarPresenter] for displaying styled snackbars
 * @param navController - [NavController] used for navigation.
 * @param dismiss - callback signalling sharing can be closed.
 */
@Suppress("TooManyFunctions")
class DefaultShareController(
    private val context: Context,
    private val shareData: List<ShareData>,
    private val sendTabUseCases: SendTabUseCases,
    private val snackbarPresenter: FenixSnackbarPresenter,
    private val navController: NavController,
    private val dismiss: (ShareController.Result) -> Unit
) : ShareController {

    override fun handleReauth() {
        val directions = ShareFragmentDirections.actionShareFragmentToAccountProblemFragment()
        navController.nav(R.id.shareFragment, directions)
        dismiss(ShareController.Result.DISMISSED)
    }

    override fun handleShareClosed() {
        dismiss(ShareController.Result.DISMISSED)
    }

    override fun handleShareToApp(app: AppShareOption) {
        val intent = Intent(ACTION_SEND).apply {
            putExtra(EXTRA_TEXT, getShareText())
            type = "text/plain"
            flags = FLAG_ACTIVITY_NEW_TASK
            setClassName(app.packageName, app.activityName)
        }

        val result = try {
            context.startActivity(intent)
            ShareController.Result.SUCCESS
        } catch (e: SecurityException) {
            snackbarPresenter.present(context.getString(R.string.share_error_snackbar))
            ShareController.Result.SHARE_ERROR
        }
        dismiss(result)
    }

    override fun handleAddNewDevice() {
        val directions = ShareFragmentDirections.actionShareFragmentToAddNewDeviceFragment()
        navController.navigate(directions)
    }

    override fun handleShareToDevice(device: Device) {
        context.metrics.track(Event.SendTab)
        shareToDevicesWithRetry { sendTabUseCases.sendToDeviceAsync(device.id, shareData.toTabData()) }
    }

    override fun handleShareToAllDevices(devices: List<Device>) {
        shareToDevicesWithRetry { sendTabUseCases.sendToAllAsync(shareData.toTabData()) }
    }

    override fun handleSignIn() {
        context.metrics.track(Event.SignInToSendTab)
        val directions = ShareFragmentDirections.actionShareFragmentToTurnOnSyncFragment()
        navController.nav(R.id.shareFragment, directions)
        dismiss(ShareController.Result.DISMISSED)
    }

    private fun shareToDevicesWithRetry(shareOperation: () -> Deferred<Boolean>) {
        // Use GlobalScope to allow the continuation of this method even if the share fragment is closed.
        GlobalScope.launch(Dispatchers.Main) {
            val result = if (shareOperation.invoke().await()) {
                showSuccess()
                ShareController.Result.SUCCESS
            } else {
                showFailureWithRetryOption { shareToDevicesWithRetry(shareOperation) }
                ShareController.Result.DISMISSED
            }
            dismiss(result)
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
        when (shareData.size) {
            1 -> getString(R.string.sync_sent_tab_snackbar)
            else -> getString(R.string.sync_sent_tabs_snackbar)
        }
    }

    @VisibleForTesting
    fun getShareText() = shareData.joinToString("\n") { data ->
        listOfNotNull(data.url, data.text).joinToString(" ")
    }

    // Navigation between app fragments uses ShareTab as arguments. SendTabUseCases uses TabData.
    @VisibleForTesting
    internal fun List<ShareData>.toTabData() = map { data ->
        TabData(title = data.title.orEmpty(), url = data.url ?: data.text?.toDataUri().orEmpty())
    }

    private fun String.toDataUri(): String {
        return "data:,${Uri.encode(this)}"
    }
}
