/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share.viewholders

import android.content.Context
import android.graphics.PorterDuff
import android.view.View
import androidx.annotation.VisibleForTesting
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.account_share_list_item.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.lib.Do
import org.mozilla.fenix.share.ShareToAccountDevicesInteractor
import org.mozilla.fenix.share.listadapters.SyncShareOption

class AccountDeviceViewHolder(
    itemView: View,
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    val interactor: ShareToAccountDevicesInteractor
) : RecyclerView.ViewHolder(itemView) {

    private val context: Context = itemView.context

    fun bind(option: SyncShareOption) {
        bindClickListeners(option)
        bindView(option)
    }

    private fun bindClickListeners(option: SyncShareOption) {
        itemView.setOnClickListener {
            Do exhaustive when (option) {
                SyncShareOption.SignIn -> interactor.onSignIn()
                SyncShareOption.AddNewDevice -> interactor.onAddNewDevice()
                is SyncShareOption.SendAll -> interactor.onShareToAllDevices(option.devices)
                is SyncShareOption.Mobile -> interactor.onShareToDevice(option.device)
                is SyncShareOption.Desktop -> interactor.onShareToDevice(option.device)
                SyncShareOption.Reconnect -> interactor.onReauth()
                SyncShareOption.Offline -> {
                    // nothing we are offline
                }
            }
        }
    }

    private fun bindView(option: SyncShareOption) {
        val (name, drawableRes, colorRes) = when (option) {
            SyncShareOption.SignIn -> Triple(
                context.getText(R.string.sync_sign_in),
                R.drawable.mozac_ic_sync,
                R.color.default_share_background
            )
            SyncShareOption.Reconnect -> Triple(
                context.getText(R.string.sync_reconnect),
                R.drawable.mozac_ic_warning,
                R.color.default_share_background
            )
            SyncShareOption.Offline -> Triple(
                context.getText(R.string.sync_offline),
                R.drawable.mozac_ic_warning,
                R.color.default_share_background
            )
            SyncShareOption.AddNewDevice -> Triple(
                context.getText(R.string.sync_connect_device),
                R.drawable.mozac_ic_new,
                R.color.default_share_background
            )
            is SyncShareOption.SendAll -> Triple(
                context.getText(R.string.sync_send_to_all),
                R.drawable.mozac_ic_select_all,
                R.color.default_share_background
            )
            is SyncShareOption.Mobile -> Triple(
                option.name,
                R.drawable.mozac_ic_device_mobile,
                R.color.device_type_mobile_background
            )
            is SyncShareOption.Desktop -> Triple(
                option.name,
                R.drawable.mozac_ic_device_desktop,
                R.color.device_type_desktop_background
            )
        }

        itemView.deviceIcon.apply {
            setImageResource(drawableRes)
            background.setColorFilter(ContextCompat.getColor(context, colorRes), PorterDuff.Mode.SRC_IN)
            drawable.setTint(ContextCompat.getColor(context, R.color.device_foreground))
        }
        itemView.isClickable = option != SyncShareOption.Offline
        itemView.deviceName.text = name
    }

    companion object {
        const val LAYOUT_ID = R.layout.account_share_list_item
    }
}
