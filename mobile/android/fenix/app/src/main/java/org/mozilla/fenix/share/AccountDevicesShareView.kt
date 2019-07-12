/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share

import android.content.Context
import android.graphics.PorterDuff.Mode.SRC_IN
import android.util.AttributeSet
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observer
import kotlinx.android.synthetic.main.account_share_list_item.view.*
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceCapability
import mozilla.components.concept.sync.DeviceType
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components

class AccountDevicesShareRecyclerView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : RecyclerView(context, attrs, defStyleAttr) {

    init {
        layoutManager = LinearLayoutManager(context, LinearLayoutManager.HORIZONTAL, false)
    }
}

class AccountDevicesShareAdapter(
    private val context: Context,
    val actionEmitter: Observer<ShareAction>
) : RecyclerView.Adapter<AccountDeviceViewHolder>() {

    private val devices = buildDeviceList()

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): AccountDeviceViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(AccountDeviceViewHolder.LAYOUT_ID, parent, false)
        return AccountDeviceViewHolder(view, actionEmitter)
    }

    override fun getItemCount(): Int = devices.size

    override fun onBindViewHolder(holder: AccountDeviceViewHolder, position: Int) {
        holder.bind(devices[position])
    }

    private fun buildDeviceList(): List<SyncShareOption> {
        val list = mutableListOf<SyncShareOption>()
        val accountManager = context.components.backgroundServices.accountManager

        if (accountManager.authenticatedAccount() == null) {
            list.add(SyncShareOption.SignIn)
            return list
        }

        accountManager.authenticatedAccount()?.deviceConstellation()?.state()?.otherDevices?.let { devices ->
            val shareableDevices = devices.filter { it.capabilities.contains(DeviceCapability.SEND_TAB) }

            if (shareableDevices.isEmpty()) {
                list.add(SyncShareOption.AddNewDevice)
            }

            val shareOptions = shareableDevices.map {
                when (it.deviceType) {
                    DeviceType.MOBILE -> SyncShareOption.Mobile(it.displayName, it)
                    else -> SyncShareOption.Desktop(it.displayName, it)
                }
            }
            list.addAll(shareOptions)

            if (shareableDevices.size > 1) {
                list.add(SyncShareOption.SendAll(shareableDevices))
            }
        }
        return list
    }
}

class AccountDeviceViewHolder(
    itemView: View,
    actionEmitter: Observer<ShareAction>
) : RecyclerView.ViewHolder(itemView) {

    private val context: Context = itemView.context
    private var action: ShareAction? = null

    init {
        itemView.setOnClickListener {
            action?.let { actionEmitter.onNext(it) }
        }
    }

    fun bind(option: SyncShareOption) {
        val (name, drawableRes, colorRes) = when (option) {
            SyncShareOption.SignIn -> {
                action = ShareAction.SignInClicked
                Triple(
                    context.getText(R.string.sync_sign_in),
                    R.drawable.mozac_ic_sync,
                    R.color.default_share_background
                )
            }
            SyncShareOption.AddNewDevice -> {
                action = ShareAction.AddNewDeviceClicked
                Triple(
                    context.getText(R.string.sync_connect_device),
                    R.drawable.mozac_ic_new,
                    R.color.default_share_background
                )
            }
            is SyncShareOption.SendAll -> {
                action = ShareAction.SendAllClicked(option.devices)
                Triple(
                    context.getText(R.string.sync_send_to_all),
                    R.drawable.mozac_ic_select_all,
                    R.color.default_share_background
                )
            }
            is SyncShareOption.Mobile -> {
                action = ShareAction.ShareDeviceClicked(option.device)
                Triple(
                    option.name,
                    R.drawable.mozac_ic_device_mobile,
                    R.color.device_type_mobile_background
                )
            }
            is SyncShareOption.Desktop -> {
                action = ShareAction.ShareDeviceClicked(option.device)
                Triple(
                    option.name,
                    R.drawable.mozac_ic_device_desktop,
                    R.color.device_type_desktop_background
                )
            }
        }

        itemView.device_icon.apply {
            setImageResource(drawableRes)
            background.setColorFilter(ContextCompat.getColor(context, colorRes), SRC_IN)
            drawable.setTint(ContextCompat.getColor(context, R.color.device_foreground))
        }
        itemView.device_name.text = name
    }

    companion object {
        const val LAYOUT_ID = R.layout.account_share_list_item
    }
}

sealed class SyncShareOption {
    object SignIn : SyncShareOption()
    object AddNewDevice : SyncShareOption()
    data class SendAll(val devices: List<Device>) : SyncShareOption()
    data class Mobile(val name: String, val device: Device) : SyncShareOption()
    data class Desktop(val name: String, val device: Device) : SyncShareOption()
}
