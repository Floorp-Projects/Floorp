/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share

import android.content.Context
import android.widget.Toast
import android.widget.Toast.LENGTH_LONG
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.R
import org.mozilla.gecko.util.ThreadUtils

/**
 * [BrowserAction] middleware reacting in response to Save to PDF related [Action]s.
 * @property context An Application context.
 */
class SaveToPDFMiddleware(
    private val context: Context,
) : Middleware<BrowserState, BrowserAction> {

    override fun invoke(
        ctx: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction,
    ) {
        if (action is EngineAction.SaveToPdfExceptionAction) {
            // See https://github.com/mozilla-mobile/fenix/issues/27649 for more details,
            // why a Toast is used here.
            ThreadUtils.runOnUiThread {
                Toast.makeText(context, R.string.unable_to_save_to_pdf_error, LENGTH_LONG).show()
            }
            Events.saveToPdfFailure.record(NoExtras())
        } else {
            next(action)
        }
    }
}
