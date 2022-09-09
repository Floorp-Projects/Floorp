/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share.listadapters

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import mozilla.components.concept.sync.Device
import org.mozilla.fenix.share.ShareToAccountDevicesInteractor
import org.mozilla.fenix.share.viewholders.AccountDeviceViewHolder

/**
 * Adapter for a list of devices that can be shared to.
 * May also display buttons to reconnect, add a device, or send to all devices.
 */
class AccountDevicesShareAdapter(
    private val interactor: ShareToAccountDevicesInteractor,
) : ListAdapter<SyncShareOption, AccountDeviceViewHolder>(DiffCallback) {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): AccountDeviceViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(AccountDeviceViewHolder.LAYOUT_ID, parent, false)

        return AccountDeviceViewHolder(view, interactor)
    }

    override fun onBindViewHolder(holder: AccountDeviceViewHolder, position: Int) {
        holder.bind(getItem(position))
    }

    private object DiffCallback : DiffUtil.ItemCallback<SyncShareOption>() {
        override fun areItemsTheSame(oldItem: SyncShareOption, newItem: SyncShareOption) =
            when (oldItem) {
                is SyncShareOption.SendAll -> newItem is SyncShareOption.SendAll
                is SyncShareOption.SingleDevice ->
                    newItem is SyncShareOption.SingleDevice && oldItem.device.id == newItem.device.id
                else -> oldItem === newItem
            }

        @Suppress("DiffUtilEquals")
        override fun areContentsTheSame(oldItem: SyncShareOption, newItem: SyncShareOption) =
            oldItem == newItem
    }
}

/**
 * Different options to be displayed by [AccountDevicesShareAdapter].
 */
sealed class SyncShareOption {
    object Reconnect : SyncShareOption()
    object Offline : SyncShareOption()
    object SignIn : SyncShareOption()
    object AddNewDevice : SyncShareOption()
    data class SendAll(val devices: List<Device>) : SyncShareOption()
    data class SingleDevice(val device: Device) : SyncShareOption()
}
