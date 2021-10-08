/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.engine

import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.InitAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.focus.utils.AppConstants
import org.mozilla.focus.utils.Features
import java.lang.IllegalStateException

/**
 * This middleware throws an exception if there's more than one tab and the feature flag for tabs
 * is disabled. It's primary purpose is to uncover situations where we unintentionally have to many
 * tabs.
 */
class TabsFeatureMiddleware : Middleware<BrowserState, BrowserAction> {
    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction
    ) {
        next(action)

        if (AppConstants.isDevOrNightlyBuild && (action is TabListAction || action is InitAction)) {
            verifyTabFeatures(context.state)
        }
    }

    private fun verifyTabFeatures(state: BrowserState) {
        if (state.tabs.size > 1 && !Features.TABS) {
            throw IllegalStateException("More than one tab even though tabs feature is disabled")
        }
    }
}
