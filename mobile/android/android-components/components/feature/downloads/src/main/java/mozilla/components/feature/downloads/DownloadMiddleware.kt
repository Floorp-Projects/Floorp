/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import android.app.DownloadManager
import android.content.Context
import android.content.Intent
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.DownloadAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext

/**
 * [Middleware] implementation for managing downloads via the provided download service. Its
 * purpose is to react to global download state changes (e.g. of [BrowserState.downloads])
 * and notify the download service, as needed.
 */
class DownloadMiddleware(
    private val applicationContext: Context,
    private val downloadServiceClass: Class<*>
) : Middleware<BrowserState, BrowserAction> {

    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction
    ) {
        next(action)

        when (action) {
            is DownloadAction.AddDownloadAction -> {
                val intent = Intent(applicationContext, downloadServiceClass)
                intent.putExtra(DownloadManager.EXTRA_DOWNLOAD_ID, action.download.id)
                applicationContext.startService(intent)
            }
        }
    }
}
