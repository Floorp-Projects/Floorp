/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.engine.middleware

import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.ExtensionsProcessAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.concept.engine.Engine
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext

/**
 * [Middleware] implementation responsible for enabling and disabling the extensions process (spawning).
 *
 * @property engine An [Engine] instance used for handling extension process spawning.
 */
internal class ExtensionsProcessMiddleware(
    private val engine: Engine,
) : Middleware<BrowserState, BrowserAction> {

    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction,
    ) {
        // Pre process actions

        next(action)

        // Post process actions
        when (action) {
            is ExtensionsProcessAction.EnabledAction -> {
                engine.enableExtensionProcessSpawning()
            }
            is ExtensionsProcessAction.DisabledAction -> {
                engine.disableExtensionProcessSpawning()
            }
            else -> {
                // no-op
            }
        }
    }
}
