/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.view.View
import androidx.core.view.updatePadding
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.browser.toolbar.MAX_URI_LENGTH
import mozilla.components.concept.tabstray.Tab
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.InactiveFooterItemBinding
import org.mozilla.fenix.databinding.InactiveHeaderItemBinding
import org.mozilla.fenix.databinding.InactiveTabListItemBinding
import org.mozilla.fenix.databinding.InactiveTabsAutoCloseBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.loadIntoView
import org.mozilla.fenix.ext.toShortUrl
import org.mozilla.fenix.home.topsites.dpToPx
import org.mozilla.fenix.tabstray.TabsTrayInteractor

sealed class InactiveTabViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {

    class HeaderHolder(
        itemView: View,
        inactiveTabsInteractor: InactiveTabsInteractor,
        tabsTrayInteractor: TabsTrayInteractor,
    ) : InactiveTabViewHolder(itemView) {

        private val binding = InactiveHeaderItemBinding.bind(itemView)

        init {
            itemView.apply {
                isActivated = InactiveTabsState.isExpanded

                correctHeaderBorder(isActivated)

                setOnClickListener {
                    val newState = !it.isActivated

                    inactiveTabsInteractor.onHeaderClicked(newState)

                    it.isActivated = newState

                    correctHeaderBorder(isActivated)
                }

                binding.delete.setOnClickListener {
                    tabsTrayInteractor.onDeleteInactiveTabs()
                }
            }
        }

        /**
         * When the header is collapsed we use its bottom border instead of the footer's
         */
        private fun correctHeaderBorder(isActivated: Boolean) {
            binding.inactiveHeaderBorder.updatePadding(
                bottom = binding.root.context.dpToPx(if (isActivated) 0f else 1f)
            )
        }

        companion object {
            const val LAYOUT_ID = R.layout.inactive_header_item
        }
    }

    class AutoCloseDialogHolder(
        itemView: View,
        interactor: InactiveTabsAutoCloseDialogInteractor
    ) : InactiveTabViewHolder(itemView) {
        private val binding = InactiveTabsAutoCloseBinding.bind(itemView)

        init {
            binding.closeButton.setOnClickListener {
                interactor.onCloseClicked()
            }

            binding.action.setOnClickListener {
                interactor.onEnabledAutoCloseClicked()
            }
        }

        companion object {
            const val LAYOUT_ID = R.layout.inactive_tabs_auto_close
        }
    }

    /**
     * A RecyclerView ViewHolder implementation for an inactive tab view.
     *
     * @param itemView the inactive tab [View].
     * @param browserTrayInteractor [BrowserTrayInteractor] handling tabs interactions in a tab tray.
     * @param featureName [String] representing the name of the feature displaying tabs. Used in telemetry reporting.
     */
    class TabViewHolder(
        itemView: View,
        private val browserTrayInteractor: BrowserTrayInteractor,
        private val featureName: String
    ) : InactiveTabViewHolder(itemView) {

        private val binding = InactiveTabListItemBinding.bind(itemView)

        fun bind(tab: Tab) {
            val components = itemView.context.components
            val title = tab.title.ifEmpty { tab.url.take(MAX_URI_LENGTH) }
            val url = tab.url.toShortUrl(components.publicSuffixList).take(MAX_URI_LENGTH)

            itemView.setOnClickListener {
                browserTrayInteractor.open(tab, featureName)
            }

            binding.siteListItem.apply {
                components.core.icons.loadIntoView(iconView, tab.url)
                setText(title, url)
                setSecondaryButton(
                    R.drawable.mozac_ic_close,
                    R.string.content_description_close_button
                ) {
                    browserTrayInteractor.close(tab, featureName)
                }
            }
        }

        companion object {
            const val LAYOUT_ID = R.layout.inactive_tab_list_item
        }
    }

    class FooterHolder(itemView: View) : InactiveTabViewHolder(itemView) {

        init {
            InactiveFooterItemBinding.bind(itemView)
        }

        companion object {
            const val LAYOUT_ID = R.layout.inactive_footer_item
        }
    }
}
