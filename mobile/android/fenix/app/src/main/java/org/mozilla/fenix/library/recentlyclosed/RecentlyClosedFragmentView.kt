/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.recentlyclosed

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.appcompat.content.res.AppCompatResources
import androidx.core.view.isVisible
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.SimpleItemAnimator
import mozilla.components.browser.state.state.recover.RecoverableTab
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.ComponentRecentlyClosedBinding
import org.mozilla.fenix.library.LibraryPageView
import org.mozilla.fenix.selection.SelectionInteractor

interface RecentlyClosedInteractor : SelectionInteractor<RecoverableTab> {
    /**
     * Called when an item is tapped to restore it.
     *
     * @param item the tapped item to restore.
     */
    fun restore(item: RecoverableTab)

    /**
     * Called when the view more history option is tapped.
     */
    fun onNavigateToHistory()

    /**
     * Copies the URL of a recently closed tab item to the copy-paste buffer.
     *
     * @param item the recently closed tab item to copy the URL from
     */
    fun onCopyPressed(item: RecoverableTab)

    /**
     * Opens the share sheet for a recently closed tab item.
     *
     * @param item the recently closed tab item to share
     */
    fun onSharePressed(item: RecoverableTab)

    /**
     * Opens a recently closed tab item in a new tab.
     *
     * @param item the recently closed tab item to open in a new tab
     */
    fun onOpenInNormalTab(item: RecoverableTab)

    /**
     * Opens a recently closed tab item in a private tab.
     *
     * @param item the recently closed tab item to open in a private tab
     */
    fun onOpenInPrivateTab(item: RecoverableTab)

    /**
     * Called when recently closed tab is selected for deletion.
     *
     * @param tab the recently closed tab to delete.
     */
    fun onDelete(tab: RecoverableTab)
}

/**
 * View that contains and configures the Recently Closed List
 */
class RecentlyClosedFragmentView(
    container: ViewGroup,
    private val interactor: RecentlyClosedFragmentInteractor
) : LibraryPageView(container) {

    private val binding = ComponentRecentlyClosedBinding.inflate(
        LayoutInflater.from(container.context), container, true
    )

    private val recentlyClosedAdapter: RecentlyClosedAdapter = RecentlyClosedAdapter(interactor)

    init {
        binding.recentlyClosedList.apply {
            layoutManager = LinearLayoutManager(containerView.context)
            adapter = recentlyClosedAdapter
            (itemAnimator as SimpleItemAnimator).supportsChangeAnimations = false
        }

        binding.viewMoreHistory.apply {
            titleView.text =
                containerView.context.getString(R.string.recently_closed_show_full_history)
            urlView.isVisible = false
            overflowView.isVisible = false
            iconView.background = null
            iconView.setImageDrawable(
                AppCompatResources.getDrawable(
                    containerView.context,
                    R.drawable.ic_history
                )
            )
            setOnClickListener {
                interactor.onNavigateToHistory()
            }
        }
    }

    fun update(state: RecentlyClosedFragmentState) {
        state.apply {
            binding.recentlyClosedEmptyView.isVisible = items.isEmpty()
            binding.recentlyClosedList.isVisible = items.isNotEmpty()

            recentlyClosedAdapter.updateData(items, selectedTabs)

            if (selectedTabs.isEmpty()) {
                setUiForNormalMode(context.getString(R.string.library_recently_closed_tabs))
            } else {
                setUiForSelectingMode(
                    context.getString(R.string.history_multi_select_title, selectedTabs.size)
                )
            }
        }
    }
}
