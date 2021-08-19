/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.downloads

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.core.view.isVisible
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.SimpleItemAnimator
import mozilla.components.support.base.feature.UserInteractionHandler
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.ComponentDownloadsBinding
import org.mozilla.fenix.library.LibraryPageView
import org.mozilla.fenix.selection.SelectionInteractor

/**
 * Interface for the DownloadViewInteractor. This interface is implemented by objects that want
 * to respond to user interaction on the DownloadView
 */
interface DownloadViewInteractor : SelectionInteractor<DownloadItem> {

    /**
     * Called on backpressed to exit edit mode
     */
    fun onBackPressed(): Boolean

    /**
     * Called when the mode is switched so we can invalidate the menu
     */
    fun onModeSwitched()

    /**
     * Called when multiple downloads items are deleted
     * @param items the downloads items to delete
     */
    fun onDeleteSome(items: Set<DownloadItem>)

    /**
     * Called when all downloads items are deleted
     */
    fun onDeleteAll()
}

/**
 * View that contains and configures the Downloads List
 */
class DownloadView(
    container: ViewGroup,
    val interactor: DownloadInteractor
) : LibraryPageView(container), UserInteractionHandler {

    val binding = ComponentDownloadsBinding.inflate(
        LayoutInflater.from(container.context),
        container,
        true
    )

    var mode: DownloadFragmentState.Mode = DownloadFragmentState.Mode.Normal
        private set

    val downloadAdapter = DownloadAdapter(interactor)
    private val layoutManager = LinearLayoutManager(container.context)

    init {
        binding.downloadList.apply {
            layoutManager = this@DownloadView.layoutManager
            adapter = downloadAdapter
            (itemAnimator as SimpleItemAnimator).supportsChangeAnimations = false
        }
    }

    fun update(state: DownloadFragmentState) {
        val oldMode = mode

        binding.progressBar.isVisible = state.isDeletingItems
        binding.swipeRefresh.isEnabled = false
        mode = state.mode

        downloadAdapter.updatePendingDeletionIds(state.pendingDeletionIds)

        updateEmptyState(state.pendingDeletionIds.size != state.items.size)

        downloadAdapter.updateMode(state.mode)
        downloadAdapter.updateDownloads(state.items)

        if (state.mode::class != oldMode::class) {
            interactor.onModeSwitched()
        }

        when (val mode = state.mode) {
            is DownloadFragmentState.Mode.Normal -> {
                setUiForNormalMode(
                    context.getString(R.string.library_downloads)
                )
            }
            is DownloadFragmentState.Mode.Editing -> {
                val unselectedItems = oldMode.selectedItems - state.mode.selectedItems

                state.mode.selectedItems.union(unselectedItems).forEach { item ->
                    val index = state.items.indexOf(item)
                    downloadAdapter.notifyItemChanged(index)
                }
                setUiForSelectingMode(
                    context.getString(R.string.download_multi_select_title, mode.selectedItems.size)
                )
            }
        }
    }

    fun updateEmptyState(userHasDownloads: Boolean) {
        binding.downloadList.isVisible = userHasDownloads
        binding.downloadEmptyView.isVisible = !userHasDownloads
        if (!userHasDownloads) {
            binding.downloadEmptyView.announceForAccessibility(context.getString(R.string.download_empty_message_1))
        }
    }

    override fun onBackPressed(): Boolean {
        return interactor.onBackPressed()
    }
}
