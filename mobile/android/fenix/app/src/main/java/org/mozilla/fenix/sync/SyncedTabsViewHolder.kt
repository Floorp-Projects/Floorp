/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.sync

import android.view.View
import android.view.View.GONE
import android.view.View.VISIBLE
import android.view.animation.Animation
import android.view.animation.AnimationUtils
import android.widget.LinearLayout
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.sync_tabs_error_row.view.*
import kotlinx.android.synthetic.main.sync_tabs_list_item.view.*
import kotlinx.android.synthetic.main.view_synced_tabs_group.view.*
import kotlinx.android.synthetic.main.view_synced_tabs_title.view.*
import mozilla.components.concept.sync.DeviceType
import mozilla.components.feature.syncedtabs.view.SyncedTabsView
import mozilla.components.support.ktx.android.util.dpToPx
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.R
import org.mozilla.fenix.sync.SyncedTabsAdapter.AdapterItem

/**
 * The various view-holders that can be found in a [SyncedTabsAdapter]. For more
 * descriptive information on the different types, see the docs for [AdapterItem].
 */
sealed class SyncedTabsViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {

    abstract fun <T : AdapterItem> bind(item: T, interactor: SyncedTabsView.Listener)

    class TabViewHolder(itemView: View) : SyncedTabsViewHolder(itemView) {

        override fun <T : AdapterItem> bind(item: T, interactor: SyncedTabsView.Listener) {
            bindTab(item as AdapterItem.Tab)

            itemView.setOnClickListener {
                interactor.onTabClicked(item.tab)
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

    class ErrorViewHolder(itemView: View) : SyncedTabsViewHolder(itemView) {

        override fun <T : AdapterItem> bind(item: T, interactor: SyncedTabsView.Listener) {
            val errorItem = item as AdapterItem.Error
            setErrorMargins()

            itemView.sync_tabs_error_description.text =
                itemView.context.getString(errorItem.descriptionResId)
            itemView.sync_tabs_error_cta_button.visibility = GONE

            errorItem.navController?.let { navController ->
                itemView.sync_tabs_error_cta_button.visibility = VISIBLE
                itemView.sync_tabs_error_cta_button.setOnClickListener {
                    navController.navigate(NavGraphDirections.actionGlobalTurnOnSync())
                }
            }
        }

        companion object {
            const val LAYOUT_ID = R.layout.sync_tabs_error_row
        }
    }

    class DeviceViewHolder(itemView: View) : SyncedTabsViewHolder(itemView) {

        override fun <T : AdapterItem> bind(item: T, interactor: SyncedTabsView.Listener) {
            bindHeader(item as AdapterItem.Device)
        }

        private fun bindHeader(device: AdapterItem.Device) {
            val deviceLogoDrawable = when (device.device.deviceType) {
                DeviceType.DESKTOP -> R.drawable.mozac_ic_device_desktop
                else -> R.drawable.mozac_ic_device_mobile
            }

            itemView.synced_tabs_group_name.text = device.device.displayName
            itemView.synced_tabs_group_name.setCompoundDrawablesWithIntrinsicBounds(
                deviceLogoDrawable,
                0,
                0,
                0
            )
        }

        companion object {
            const val LAYOUT_ID = R.layout.view_synced_tabs_group
        }
    }

    class NoTabsViewHolder(itemView: View) : SyncedTabsViewHolder(itemView) {
        override fun <T : AdapterItem> bind(item: T, interactor: SyncedTabsView.Listener) = Unit

        companion object {
            const val LAYOUT_ID = R.layout.view_synced_tabs_no_item
        }
    }

    class TitleViewHolder(itemView: View) : SyncedTabsViewHolder(itemView) {

        override fun <T : AdapterItem> bind(item: T, interactor: SyncedTabsView.Listener) {
            itemView.refresh_icon.setOnClickListener { v ->
                val rotation = AnimationUtils.loadAnimation(
                    itemView.context,
                    R.anim.full_rotation
                ).apply {
                    repeatCount = Animation.ABSOLUTE
                }

                v.startAnimation(rotation)

                interactor.onRefresh()
            }
        }

        companion object {
            const val LAYOUT_ID = R.layout.view_synced_tabs_title
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
