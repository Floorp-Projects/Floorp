/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.loader

import android.content.Context
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.browser.icons.IconRequest
import mozilla.components.concept.fetch.Client

/**
 * [HttpIconLoader] variation that will immediately resolve a [load] request with [IconLoader.Result.NoResult]
 * and then continue to actually download the icon in the background finally calling [loadCallback]
 * with the actual result and details about the request.
 *
 * @property httpClient [Client] used for downloading the icon.
 * @property scope [CoroutineScope] used for downloading the icon in the background.
 * Defaults to a new scope using [Dispatchers.IO] for allowing multiple requests to block their threads
 * while waiting for the download to complete.
 * @property loadCallback Callback for when the network icon finished downloading or an error or timeout occurred.
 */
class NonBlockingHttpIconLoader(
    httpClient: Client,
    private val scope: CoroutineScope = CoroutineScope(Dispatchers.IO),
    private val loadCallback: (IconRequest, IconRequest.Resource, IconLoader.Result) -> Unit,
) : HttpIconLoader(httpClient) {
    override fun load(context: Context, request: IconRequest, resource: IconRequest.Resource): IconLoader.Result {
        if (!shouldDownload(resource)) {
            return IconLoader.Result.NoResult
        }

        scope.launch {
            val icon = internalLoad(request, resource)

            loadCallback(request, resource, icon)
        }

        return IconLoader.Result.NoResult
    }
}
