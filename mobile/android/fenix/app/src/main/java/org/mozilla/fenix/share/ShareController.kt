/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share

import android.content.Intent
import android.content.Intent.ACTION_SEND
import android.content.Intent.EXTRA_TEXT
import android.content.Intent.FLAG_ACTIVITY_NEW_TASK
import androidx.annotation.VisibleForTesting
import androidx.appcompat.app.AlertDialog
import androidx.fragment.app.Fragment
import androidx.navigation.NavController
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.TabData
import mozilla.components.feature.sendtab.SendTabUseCases
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.share.listadapters.AppShareOption

/**
 * [ShareFragment] controller.
 *
 * Delegated by View Interactors, handles container business logic and operates changes on it.
 */
interface ShareController {
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
 * @param fragment the [ShareFragment] instance this controller handles business logic for.
 * @param sharedTabs the list of [ShareTab]s that can be shared.
 * @param sendTabUseCases instance of [SendTabUseCases] which allows sending tabs to account devices.
 * @param navController - [NavController] used for navigation.
 * @param dismiss - callback signalling sharing can be closed.
 */
class DefaultShareController(
    private val fragment: Fragment,
    private val sharedTabs: List<ShareTab>,
    private val sendTabUseCases: SendTabUseCases,
    private val navController: NavController,
    private val dismiss: () -> Unit
) : ShareController {
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
        fragment.startActivity(intent)
        dismiss()
    }

    override fun handleAddNewDevice() {
        AlertDialog.Builder(fragment.requireContext()).apply {
            setMessage(R.string.sync_connect_device_dialog)
            setPositiveButton(R.string.sync_confirmation_button) { dialog, _ -> dialog.cancel() }
            create()
        }.show()
    }

    override fun handleShareToDevice(device: Device) {
        sendTabUseCases.sendToDeviceAsync(device.id, sharedTabs.toTabData())
        (fragment.activity as ShareFragment.TabsSharedCallback).onTabsShared(sharedTabs.size)
        dismiss()
    }

    override fun handleShareToAllDevices(devices: List<Device>) {
        sendTabUseCases.sendToAllAsync(sharedTabs.toTabData())
        (fragment.activity as ShareFragment.TabsSharedCallback).onTabsShared(sharedTabs.size)
        dismiss()
    }

    override fun handleSignIn() {
        val directions = ShareFragmentDirections.actionShareFragmentToTurnOnSyncFragment()
        navController.nav(R.id.shareFragment, directions)
        dismiss()
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    fun getShareText() = sharedTabs.joinToString("\n") { tab -> tab.url }

    // Navigation between app fragments uses ShareTab as arguments. SendTabUseCases uses TabData.
    private fun ShareTab.toTabData() = TabData(title, url)
    private fun List<ShareTab>.toTabData() = map { it.toTabData() }
}
