/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.file

import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext

/**
 * [Middleware] that observe when a user navigates away from a site and clean up,
 * temporary file uploads.
 */
class FileUploadsDirCleanerMiddleware(
    private val fileUploadsDirCleaner: FileUploadsDirCleaner,
) : Middleware<BrowserState, BrowserAction> {

    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction,
    ) {
        when (action) {
            is TabListAction.SelectTabAction,
            is ContentAction.UpdateUrlAction,
            is ContentAction.UpdateLoadRequestAction,
            -> {
                fileUploadsDirCleaner.cleanRecentUploads()
            }
            else -> {
                // no-op
            }
        }
        next(action)
    }
}
