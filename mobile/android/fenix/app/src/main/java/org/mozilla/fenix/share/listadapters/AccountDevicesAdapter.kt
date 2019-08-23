/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share.listadapters

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.concept.sync.Device
import org.mozilla.fenix.share.ShareToAccountDevicesInteractor
import org.mozilla.fenix.share.viewholders.AccountDeviceViewHolder

class AccountDevicesShareAdapter(
    private val interactor: ShareToAccountDevicesInteractor,
    private val devices: MutableList<SyncShareOption> = mutableListOf()
) : RecyclerView.Adapter<AccountDeviceViewHolder>() {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): AccountDeviceViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(AccountDeviceViewHolder.LAYOUT_ID, parent, false)

        return AccountDeviceViewHolder(view, interactor)
    }

    override fun getItemCount(): Int = devices.size

    override fun onBindViewHolder(holder: AccountDeviceViewHolder, position: Int) {
        holder.bind(devices[position])
    }

    fun updateData(deviceOptions: List<SyncShareOption>) {
        this.devices.clear()
        this.devices.addAll(deviceOptions)
        notifyDataSetChanged()
    }
}

sealed class SyncShareOption {
    object SignIn : SyncShareOption()
    object AddNewDevice : SyncShareOption()
    data class SendAll(val devices: List<Device>) : SyncShareOption()
    data class Mobile(val name: String, val device: Device) : SyncShareOption()
    data class Desktop(val name: String, val device: Device) : SyncShareOption()
}
