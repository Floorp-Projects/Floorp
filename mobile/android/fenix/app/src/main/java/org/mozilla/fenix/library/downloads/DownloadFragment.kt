/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.downloads

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.annotation.VisibleForTesting
import kotlinx.android.synthetic.main.fragment_downloads.view.*
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.feature.downloads.AbstractFetchDownloadService
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.support.base.feature.UserInteractionHandler
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.filterNotExistsOnDisk
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.library.LibraryPageFragment

@SuppressWarnings("TooManyFunctions", "LargeClass")
class DownloadFragment : LibraryPageFragment<DownloadItem>(), UserInteractionHandler {
    private lateinit var downloadStore: DownloadFragmentStore
    private lateinit var downloadView: DownloadView
    private lateinit var downloadInteractor: DownloadInteractor

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val view = inflater.inflate(R.layout.fragment_downloads, container, false)

        val items = provideDownloads(requireComponents.core.store.state)

        downloadStore = StoreProvider.get(this) {
            DownloadFragmentStore(
                DownloadFragmentState(
                    items = items,
                    mode = DownloadFragmentState.Mode.Normal
                )
            )
        }

        val downloadController: DownloadController = DefaultDownloadController(
            downloadStore,
            ::openItem
        )
        downloadInteractor = DownloadInteractor(
            downloadController
        )
        downloadView = DownloadView(view.downloadsLayout, downloadInteractor)

        return view
    }

    @VisibleForTesting
    internal fun provideDownloads(state: BrowserState): List<DownloadItem> {
        return state.downloads.values
            .sortedByDescending { it.createdTime } // sort from newest to oldest
            .map {
                DownloadItem(
                    it.id,
                    it.fileName,
                    it.filePath,
                    it.contentLength.toString(),
                    it.contentType,
                    it.status
                )
            }.filter {
                it.status == DownloadState.Status.COMPLETED
            }.filterNotExistsOnDisk()
    }

    override val selectedItems get() = downloadStore.state.mode.selectedItems

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        requireComponents.analytics.metrics.track(Event.HistoryOpened)

        setHasOptionsMenu(false)
    }

    @ExperimentalCoroutinesApi
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        consumeFrom(downloadStore) {
            downloadView.update(it)
        }
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.library_downloads))
    }

    override fun onBackPressed(): Boolean {
        return downloadView.onBackPressed()
    }

    private fun openItem(item: DownloadItem, mode: BrowsingMode? = null) {

        mode?.let { (activity as HomeActivity).browsingModeManager.mode = it }
        context?.let {
            AbstractFetchDownloadService.openFile(
                context = it,
                contentType = item.contentType,
                filePath = item.filePath
            )
        }
    }
}
