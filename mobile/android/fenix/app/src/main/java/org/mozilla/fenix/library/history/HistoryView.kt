/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.core.view.isVisible
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.SimpleItemAnimator
import mozilla.components.support.base.feature.UserInteractionHandler
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.ComponentHistoryBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.library.LibraryPageView
import org.mozilla.fenix.theme.ThemeManager

/**
 * View that contains and configures the History List
 */
class HistoryView(
    container: ViewGroup,
    val interactor: HistoryInteractor
) : LibraryPageView(container), UserInteractionHandler {

    val binding = ComponentHistoryBinding.inflate(
        LayoutInflater.from(container.context), container, true
    )

    var mode: HistoryFragmentState.Mode = HistoryFragmentState.Mode.Normal
        private set

    val historyAdapter = HistoryAdapter(interactor)
    private val layoutManager = LinearLayoutManager(container.context)

    init {
        binding.historyList.apply {
            layoutManager = this@HistoryView.layoutManager
            adapter = historyAdapter
            (itemAnimator as SimpleItemAnimator).supportsChangeAnimations = false
        }

        val primaryTextColor =
            ThemeManager.resolveAttribute(R.attr.primaryText, context)
        binding.swipeRefresh.setColorSchemeColors(primaryTextColor)
        binding.swipeRefresh.setOnRefreshListener {
            interactor.onRequestSync()
            binding.historyList.scrollToPosition(0)
        }
    }

    fun update(state: HistoryFragmentState) {
        val oldMode = mode

        binding.progressBar.isVisible = state.isDeletingItems
        binding.swipeRefresh.isRefreshing = state.mode === HistoryFragmentState.Mode.Syncing
        binding.swipeRefresh.isEnabled =
            state.mode === HistoryFragmentState.Mode.Normal || state.mode === HistoryFragmentState.Mode.Syncing
        mode = state.mode

        historyAdapter.updatePendingDeletionIds(state.pendingDeletionIds)

        updateEmptyState(state.pendingDeletionIds.size != historyAdapter.currentList?.size)

        historyAdapter.updateMode(state.mode)
        val first = layoutManager.findFirstVisibleItemPosition()
        val last = layoutManager.findLastVisibleItemPosition() + 1
        historyAdapter.notifyItemRangeChanged(first, last - first)

        if (state.mode::class != oldMode::class) {
            interactor.onModeSwitched()
        }

        if (state.mode is HistoryFragmentState.Mode.Editing) {
            val unselectedItems = oldMode.selectedItems - state.mode.selectedItems

            state.mode.selectedItems.union(unselectedItems).forEach { item ->
                historyAdapter.notifyItemChanged(item.position)
            }
        }

        when (val mode = state.mode) {
            is HistoryFragmentState.Mode.Normal -> {
                setUiForNormalMode(
                    context.getString(R.string.library_history)
                )
            }
            is HistoryFragmentState.Mode.Editing -> {
                setUiForSelectingMode(
                    context.getString(R.string.history_multi_select_title, mode.selectedItems.size)
                )
            }
            else -> {
                // no-op
            }
        }
    }

    fun updateEmptyState(userHasHistory: Boolean) {
        binding.historyList.isVisible = userHasHistory
        binding.historyEmptyView.isVisible = !userHasHistory
        with(binding.recentlyClosedNavEmpty) {
            recentlyClosedNav.setOnClickListener {
                interactor.onRecentlyClosedClicked()
            }
            val numRecentTabs = recentlyClosedNav.context.components.core.store.state.closedTabs.size
            recentlyClosedTabsDescription.text = String.format(
                context.getString(
                    if (numRecentTabs == 1)
                        R.string.recently_closed_tab else R.string.recently_closed_tabs
                ),
                numRecentTabs
            )
            recentlyClosedNav.isVisible = !userHasHistory
        }
        if (!userHasHistory) {
            binding.historyEmptyView.announceForAccessibility(context.getString(R.string.history_empty_message))
        }
    }

    override fun onBackPressed(): Boolean {
        return interactor.onBackPressed()
    }
}
