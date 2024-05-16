/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.downloads

import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.distinctUntilChangedBy
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.downloads.toMegabyteOrKilobyteString
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.Store
import mozilla.components.lib.state.ext.flow
import org.mozilla.fenix.ext.filterExistsOnDisk

/**
 * Middleware for loading and mapping download items from the browser store.
 *
 * @param browserStore [BrowserStore] instance to get the download items from.
 * @param scope The [CoroutineScope] that will be used to launch coroutines.
 * @param ioDispatcher The [CoroutineDispatcher] that will be used for IO operations.
 */
class DownloadFragmentDataMiddleware(
    private val browserStore: BrowserStore,
    private val scope: CoroutineScope,
    private val ioDispatcher: CoroutineDispatcher = Dispatchers.IO,
) : Middleware<DownloadFragmentState, DownloadFragmentAction> {

    override fun invoke(
        context: MiddlewareContext<DownloadFragmentState, DownloadFragmentAction>,
        next: (DownloadFragmentAction) -> Unit,
        action: DownloadFragmentAction,
    ) {
        next(action)
        when (action) {
            is DownloadFragmentAction.Init -> update(context.store)
            else -> {
                // no - op
            }
        }
    }

    private fun update(store: Store<DownloadFragmentState, DownloadFragmentAction>) {
        scope.launch {
            browserStore.flow()
                .distinctUntilChangedBy { it.downloads }
                .map { it.downloads.toDownloadItemsList() }
                .map { it.filterExistsOnDisk(ioDispatcher) }
                .collect {
                    store.dispatch(DownloadFragmentAction.UpdateDownloadItems(it))
                }
        }
    }

    private fun Map<String, DownloadState>.toDownloadItemsList() =
        values
            .distinctBy { it.fileName }
            .sortedByDescending { it.createdTime } // sort from newest to oldest
            .map { it.toDownloadItem() }
            .filter { it.status == DownloadState.Status.COMPLETED }

    private fun DownloadState.toDownloadItem() =
        DownloadItem(
            id = id,
            url = url,
            fileName = fileName,
            filePath = filePath,
            formattedSize = contentLength?.toMegabyteOrKilobyteString() ?: "0",
            contentType = contentType,
            status = status,
        )
}
