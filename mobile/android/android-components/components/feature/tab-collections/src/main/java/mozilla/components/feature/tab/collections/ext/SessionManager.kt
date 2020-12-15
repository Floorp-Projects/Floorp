/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tab.collections.ext

import android.content.Context
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.tab.collections.Tab
import mozilla.components.feature.tab.collections.TabCollection

/**
 * Restores the given [Tab] from a [TabCollection]. Will invoke [onTabRestored] on successful restore
 * and [onFailure] otherwise.
 */
fun SessionManager.restore(
    context: Context,
    engine: Engine,
    tab: Tab,
    onTabRestored: () -> Unit,
    onFailure: () -> Unit
) {
    val item = tab.restore(
        context = context,
        engine = engine,
        restoreSessionId = false
    )

    if (item == null) {
        // We were unable to restore the tab. Let the app know so that it can workaround that
        onFailure()
    } else {
        add(
            session = item.session,
            selected = true,
            engineSessionState = item.engineSessionState
        )
        onTabRestored()
    }
}

/**
 * Restores the given [TabCollection]. Will invoke [onFailure] if restoring a single [Tab] of the
 * collection failed. The URL of the tab will be passed to [onFailure].
 */
fun SessionManager.restore(
    context: Context,
    engine: Engine,
    collection: TabCollection,
    onFailure: (String) -> Unit
) {
    collection.tabs.reversed().forEach { tab ->
        val item = tab.restore(
            context = context,
            engine = engine,
            restoreSessionId = false
        )

        if (item == null) {
            // We were unable to restore the tab. Let the app know so that it can workaround that
            onFailure(tab.url)
        } else {
            add(
                session = item.session,
                selected = selectedSession == null,
                engineSessionState = item.engineSessionState
            )
        }
    }
}
