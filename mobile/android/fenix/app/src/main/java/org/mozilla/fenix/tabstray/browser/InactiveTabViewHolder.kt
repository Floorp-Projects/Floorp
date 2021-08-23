/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.view.View
import androidx.annotation.StringRes
import androidx.core.view.updatePadding
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.browser.toolbar.MAX_URI_LENGTH
import mozilla.components.concept.tabstray.Tab
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.InactiveFooterItemBinding
import org.mozilla.fenix.databinding.InactiveRecentlyClosedItemBinding
import org.mozilla.fenix.databinding.InactiveHeaderItemBinding
import org.mozilla.fenix.databinding.InactiveTabListItemBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.loadIntoView
import org.mozilla.fenix.ext.toShortUrl
import org.mozilla.fenix.home.sessioncontrol.viewholders.topsites.dpToPx
import org.mozilla.fenix.tabstray.browser.AutoCloseInterval.Manual
import org.mozilla.fenix.tabstray.browser.AutoCloseInterval.OneDay
import org.mozilla.fenix.tabstray.browser.AutoCloseInterval.OneMonth
import org.mozilla.fenix.tabstray.browser.AutoCloseInterval.OneWeek

sealed class InactiveTabViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {

    class HeaderHolder(
        itemView: View,
        interactor: InactiveTabsInteractor
    ) : InactiveTabViewHolder(itemView) {

        private val binding = InactiveHeaderItemBinding.bind(itemView)

        init {
            itemView.apply {
                isActivated = InactiveTabsState.isExpanded

                setOnClickListener {
                    val newState = !it.isActivated

                    interactor.onHeaderClicked(newState)

                    it.isActivated = newState
                    binding.chevron.rotation = ROTATION_DEGREE

                    // When the header is collapsed we use its bottom border instead of the footer's
                    binding.inactiveHeaderBorder.updatePadding(
                        bottom = binding.root.context.dpToPx(if (it.isActivated) 0f else 1f)
                    )
                }
            }
        }

        companion object {
            const val LAYOUT_ID = R.layout.inactive_header_item
            private const val ROTATION_DEGREE = 180F
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

    class RecentlyClosedHolder(
        itemView: View,
        private val browserTrayInteractor: BrowserTrayInteractor,
    ) : InactiveTabViewHolder(itemView) {

        val binding = InactiveRecentlyClosedItemBinding.bind(itemView)

        fun bind() {
            val context = itemView.context
            binding.inactiveRecentlyClosedText.text =
                context.getString(R.string.tab_tray_inactive_recently_closed)

            binding.inactiveRecentlyClosed.setOnClickListener {
                browserTrayInteractor.onRecentlyClosedClicked()
            }
        }

        companion object {
            const val LAYOUT_ID = R.layout.inactive_recently_closed_item
        }
    }

    class FooterHolder(itemView: View) : InactiveTabViewHolder(itemView) {

        val binding = InactiveFooterItemBinding.bind(itemView)

        fun bind(interval: AutoCloseInterval) {
            val context = itemView.context
            val stringRes = when (interval) {
                Manual, OneDay -> {
                    binding.inactiveDescription.visibility = View.GONE
                    binding.topDivider.visibility = View.GONE
                    null
                }
                OneWeek -> {
                    context.getString(interval.description)
                }
                OneMonth -> {
                    context.getString(interval.description)
                }
            }
            if (stringRes != null) {
                binding.inactiveDescription.text =
                    context.getString(R.string.inactive_tabs_description, stringRes)
            }
        }

        companion object {
            const val LAYOUT_ID = R.layout.inactive_footer_item
        }
    }
}

enum class AutoCloseInterval(@StringRes val description: Int) {
    Manual(0),
    OneDay(0),
    OneWeek(R.string.inactive_tabs_7_days),
    OneMonth(R.string.inactive_tabs_30_days)
}
