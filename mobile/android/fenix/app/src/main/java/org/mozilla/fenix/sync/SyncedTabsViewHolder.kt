/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.sync

import android.view.View
import android.view.View.GONE
import android.widget.LinearLayout
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.no_content_message_with_action.view.*
import kotlinx.android.synthetic.main.sync_tabs_list_item.view.*
import kotlinx.android.synthetic.main.view_synced_tabs_group.view.*
import mozilla.components.browser.storage.sync.Tab
import mozilla.components.concept.sync.DeviceType
import mozilla.components.support.ktx.android.util.dpToPx
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.R
import org.mozilla.fenix.sync.SyncedTabsAdapter.AdapterItem

sealed class SyncedTabsViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {

    abstract fun <T : AdapterItem> bind(item: T, interactor: (Tab) -> Unit)

    class TabViewHolder(itemView: View) : SyncedTabsViewHolder(itemView) {

        override fun <T : AdapterItem> bind(item: T, interactor: (Tab) -> Unit) {
            bindTab(item as AdapterItem.Tab)

            itemView.setOnClickListener {
                interactor(item.tab)
            }
        }

        private fun bindTab(tab: AdapterItem.Tab) {
            val active = tab.tab.active()
            itemView.synced_tab_item_title.text = active.title
            itemView.synced_tab_item_url.text = active.url
        }

        companion object {
            const val LAYOUT_ID = R.layout.sync_tabs_list_item
        }
    }

    class SignInViewHolder(itemView: View) : SyncedTabsViewHolder(itemView) {

        override fun <T : AdapterItem> bind(item: T, interactor: (Tab) -> Unit) {
            val signInItem = item as AdapterItem.SignIn
            setErrorMargins()

            itemView.no_content_header.visibility = GONE
            itemView.no_content_description.text =
                itemView.context.getString(R.string.synced_tabs_sign_in_message)
            itemView.no_content_button.text =
                itemView.context.getString(R.string.synced_tabs_sign_in_button)
            itemView.no_content_button.icon =
                ContextCompat.getDrawable(itemView.context, R.drawable.ic_sign_in)
            itemView.no_content_button.setOnClickListener {
                signInItem.navController.navigate(NavGraphDirections.actionGlobalTurnOnSync())
            }
        }

        companion object {
            const val LAYOUT_ID = R.layout.no_content_message_with_action
        }
    }

    class ErrorViewHolder(itemView: View) : SyncedTabsViewHolder(itemView) {

        override fun <T : AdapterItem> bind(item: T, interactor: (Tab) -> Unit) {
            val errorItem = item as AdapterItem.Error
            setErrorMargins()

            itemView.no_content_header.visibility = GONE
            itemView.no_content_description.text = itemView.context.getString(errorItem.errorResId)
        }

        companion object {
            const val LAYOUT_ID = R.layout.no_content_message
        }
    }

    class DeviceViewHolder(itemView: View) : SyncedTabsViewHolder(itemView) {

        override fun <T : AdapterItem> bind(item: T, interactor: (Tab) -> Unit) {
            bindHeader(item as AdapterItem.Device)
        }

        private fun bindHeader(device: AdapterItem.Device) {
            val deviceLogoDrawable = when (device.device.deviceType) {
                DeviceType.DESKTOP -> R.drawable.mozac_ic_device_desktop
                else -> R.drawable.mozac_ic_device_mobile
            }

            itemView.synced_tabs_group_name.text = device.device.displayName
            itemView.synced_tabs_group_name.setCompoundDrawablesWithIntrinsicBounds(deviceLogoDrawable, 0, 0, 0)
        }

        companion object {
            const val LAYOUT_ID = R.layout.view_synced_tabs_group
        }
    }

    internal fun setErrorMargins() {
        val lp = LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT,
            LinearLayout.LayoutParams.WRAP_CONTENT
        )
        val displayMetrics = itemView.context.resources.displayMetrics
        val margin = ERROR_MARGIN.dpToPx(displayMetrics)
        lp.setMargins(margin, margin, margin, 0)
        itemView.layoutParams = lp
    }

    companion object {
        private const val ERROR_MARGIN = 20
    }
}
