/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.exceptions

import android.content.Context
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.focus.components.EngineProvider

/**
 * [Middleware] implementation for migrating the tracking protection exceptions: Loading the list of
 * exceptions from SharedPreferences and passing them to GeckoView.
 *
 * This is a workaround until GeckoView can permanently save tracking protection exceptions for
 * private tabs. Otherwise they get dropped once the last GeckoSession gets closed or the process
 * shuts down.
 */
class ExceptionMigrationMiddleware(
    private val context: Context
) : Middleware<BrowserState, BrowserAction> {
    private val migrator by lazy { EngineProvider.provideTrackingProtectionMigrator(context) }
    private var hasTabs = false

    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction
    ) {
        next(action)

        if (action is TabListAction) {
            checkIfMigrationIsNeeded(context.state)
        }
    }

    private fun checkIfMigrationIsNeeded(state: BrowserState) {
        if (state.privateTabs.isEmpty() && hasTabs) {
            // All tabs have been removed, let's remember this state, so that we can migrate the
            // tracking protection exceptions again, once the first new tab gets added. We have to
            // delay the migration because we do not know when the matching GeckoSession will get
            // closed, because that happens asynchronously.
            hasTabs = false
        } else if (state.privateTabs.isNotEmpty() && !hasTabs) {
            // There are tabs now, let's migrate the exceptions again.
            migrateTrackingProtectionExceptions()
            hasTabs = true
        }
    }

    private fun migrateTrackingProtectionExceptions() {
        // We migrate the tracking protection exceptions blocking the store thread, because we want
        // to make sure this has happened before the first GeckoSession gets created.
        runBlocking {
            migrator.start(context)
        }
    }
}
