/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.view.View
import androidx.core.view.updatePadding
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.tabstray.TabsTray
import mozilla.components.browser.toolbar.MAX_URI_LENGTH
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.databinding.InactiveFooterItemBinding
import org.mozilla.fenix.databinding.InactiveHeaderItemBinding
import org.mozilla.fenix.databinding.InactiveTabListItemBinding
import org.mozilla.fenix.databinding.InactiveTabsAutoCloseBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.loadIntoView
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.ext.toShortUrl
import org.mozilla.fenix.home.topsites.dpToPx
import org.mozilla.fenix.tabstray.TabsTrayFragment
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
                isActivated = itemView.context.components.appStore.state.inactiveTabsExpanded

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
            binding.root.context.metrics.track(Event.TabsTrayAutoCloseDialogSeen)

            binding.message.text = with(binding.root.context) {
                getString(
                    R.string.tab_tray_inactive_auto_close_body_2,
                    getString(R.string.app_name)
                )
            }

            binding.closeButton.setOnClickListener {
                interactor.onCloseClicked()
            }

            binding.action.setOnClickListener {
                interactor.onEnabledAutoCloseClicked()
                showConfirmationSnackbar()
            }
        }

        private fun showConfirmationSnackbar() {
            val context = binding.root.context
            val view = binding.root
            val text = context.getString(R.string.inactive_tabs_auto_close_message_snackbar)
            val snackbar = FenixSnackbar.make(
                view = view,
                duration = FenixSnackbar.LENGTH_SHORT,
                isDisplayedWithBrowserToolbar = true
            ).setText(text)
            snackbar.view.elevation = TabsTrayFragment.ELEVATION
            snackbar.show()
        }

        companion object {
            const val LAYOUT_ID = R.layout.inactive_tabs_auto_close
        }
    }

    /**
     * A RecyclerView ViewHolder implementation for an inactive tab view.
     *
     * @param itemView the inactive tab [View].
     * @param featureName [String] representing the name of the feature displaying tabs. Used in telemetry reporting.
     */
    class TabViewHolder(
        itemView: View,
        private val delegate: TabsTray.Delegate,
        private val featureName: String
    ) : InactiveTabViewHolder(itemView) {

        private val binding = InactiveTabListItemBinding.bind(itemView)

        fun bind(tab: TabSessionState) {
            val components = itemView.context.components
            val title = tab.content.title.ifEmpty { tab.content.url.take(MAX_URI_LENGTH) }
            val url = tab.content.url.toShortUrl(components.publicSuffixList).take(MAX_URI_LENGTH)

            itemView.setOnClickListener {
                components.analytics.metrics.track(Event.TabsTrayOpenInactiveTab)
                delegate.onTabSelected(tab, featureName)
            }

            binding.siteListItem.apply {
                components.core.icons.loadIntoView(iconView, tab.content.url)
                setText(title, url)
                setSecondaryButton(
                    R.drawable.mozac_ic_close,
                    R.string.content_description_close_button
                ) {
                    components.analytics.metrics.track(Event.TabsTrayCloseInactiveTab())
                    delegate.onTabClosed(tab, featureName)
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
