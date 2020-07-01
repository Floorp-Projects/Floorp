

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.downloads

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.SimpleItemAnimator
import kotlinx.android.synthetic.main.component_downloads.view.*
import mozilla.components.support.base.feature.UserInteractionHandler
import org.mozilla.fenix.R
import org.mozilla.fenix.library.LibraryPageView
import org.mozilla.fenix.library.SelectionInteractor

/**
 * Interface for the DownloadViewInteractor. This interface is implemented by objects that want
 * to respond to user interaction on the DownloadView
 */
interface DownloadViewInteractor : SelectionInteractor<DownloadItem> {

    /**
     * Called on backpressed to exit edit mode
     */
    fun onBackPressed(): Boolean
}

/**
 * View that contains and configures the Downloads List
 */
class DownloadView(
    container: ViewGroup,
    val interactor: DownloadInteractor
) : LibraryPageView(container), UserInteractionHandler {

    val view: View = LayoutInflater.from(container.context)
        .inflate(R.layout.component_downloads, container, true)

    var mode: DownloadFragmentState.Mode = DownloadFragmentState.Mode.Normal
        private set

    val downloadAdapter = DownloadAdapter(interactor)
    private val layoutManager = LinearLayoutManager(container.context)

    init {
        view.download_list.apply {
            layoutManager = this@DownloadView.layoutManager
            adapter = downloadAdapter
            (itemAnimator as SimpleItemAnimator).supportsChangeAnimations = false
        }
    }

    fun update(state: DownloadFragmentState) {

        view.swipe_refresh.isEnabled =
            state.mode === DownloadFragmentState.Mode.Normal
        mode = state.mode

        downloadAdapter.updateMode(state.mode)
        downloadAdapter.updateDownloads(state.items)

        setUiForNormalMode(
            context.getString(R.string.library_downloads)
        )
    }

    override fun onBackPressed(): Boolean {
        return interactor.onBackPressed()
    }
}
