/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.sync

import android.view.View
import android.view.View.GONE
import android.view.View.VISIBLE
import android.view.animation.Animation
import android.view.animation.AnimationUtils
import androidx.annotation.VisibleForTesting
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.browser.toolbar.MAX_URI_LENGTH
import mozilla.components.feature.syncedtabs.view.SyncedTabsView
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.SyncTabsErrorRowBinding
import org.mozilla.fenix.databinding.SyncTabsListItemBinding
import org.mozilla.fenix.databinding.ViewSyncedTabsGroupBinding
import org.mozilla.fenix.databinding.ViewSyncedTabsTitleBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.toShortUrl
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
            val binding = SyncTabsListItemBinding.bind(itemView)
            binding.syncedTabItemTitle.text = active.title
            binding.syncedTabItemUrl.text = active.url
                .toShortUrl(itemView.context.components.publicSuffixList)
                .take(MAX_URI_LENGTH)
        }

        companion object {
            const val LAYOUT_ID = R.layout.sync_tabs_list_item
        }
    }

    class ErrorViewHolder(itemView: View) : SyncedTabsViewHolder(itemView) {

        override fun <T : AdapterItem> bind(item: T, interactor: SyncedTabsView.Listener) {
            val errorItem = item as AdapterItem.Error
            val binding = SyncTabsErrorRowBinding.bind(itemView)

            binding.syncTabsErrorDescription.text =
                itemView.context.getString(errorItem.descriptionResId)
            binding.syncTabsErrorCtaButton.visibility = GONE

            errorItem.navController?.let { navController ->
                binding.syncTabsErrorCtaButton.visibility = VISIBLE
                binding.syncTabsErrorCtaButton.setOnClickListener {
                    navController.navigate(NavGraphDirections.actionGlobalTurnOnSync())
                }
            }
        }

        companion object {
            const val LAYOUT_ID = R.layout.sync_tabs_error_row
        }
    }

    class DeviceViewHolder(itemView: View) : SyncedTabsViewHolder(itemView) {

        @VisibleForTesting
        internal val binding = ViewSyncedTabsGroupBinding.bind(itemView)

        override fun <T : AdapterItem> bind(item: T, interactor: SyncedTabsView.Listener) {
            bindHeader(item as AdapterItem.Device)
        }

        private fun bindHeader(device: AdapterItem.Device) {
            binding.syncedTabsGroupName.text = device.device.displayName
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
            val binding = ViewSyncedTabsTitleBinding.bind(itemView)
            binding.refreshIcon.setOnClickListener { v ->
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
}
