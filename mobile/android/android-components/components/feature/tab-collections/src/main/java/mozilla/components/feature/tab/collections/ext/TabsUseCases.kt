/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tab.collections.ext

import android.content.Context
import mozilla.components.browser.state.action.LastAccessAction
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.tab.collections.Tab
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.tabs.TabsUseCases

/**
 * Restores the given [Tab] from a [TabCollection]. Will invoke [onTabRestored] on successful restore
 * and [onFailure] otherwise.
 *
 * Will update the last accessed property of the tab if [updateLastAccess] is true.
 */
operator fun TabsUseCases.RestoreUseCase.invoke(
    context: Context,
    engine: Engine,
    tab: Tab,
    updateLastAccess: Boolean = true,
    onTabRestored: (String) -> Unit,
    onFailure: () -> Unit,
) {
    val item = tab.restore(
        context = context,
        engine = engine,
        restoreSessionId = false,
    )

    if (item == null) {
        // We were unable to restore the tab. Let the app know so that it can workaround that
        onFailure()
    } else {
        invoke(listOf(item), item.state.id)

        if (updateLastAccess) {
            store.dispatch(LastAccessAction.UpdateLastAccessAction(item.state.id))
        }

        onTabRestored(item.state.id)
    }
}

/**
 * Restores the given [TabCollection].
 *
 * Will invoke [onFailure] if restoring a single [Tab] of the collection failed. The URL of the
 * tab will be passed to [onFailure].
 *
 * Will update the last accessed property of the tab if [updateLastAccess] is true.
 */
operator fun TabsUseCases.RestoreUseCase.invoke(
    context: Context,
    engine: Engine,
    collection: TabCollection,
    updateLastAccess: Boolean = true,
    onFailure: (String) -> Unit,
) {
    val tabs = collection.tabs.reversed().mapNotNull { tab ->
        val recoverableTab = tab.restore(context, engine, restoreSessionId = false)
        if (recoverableTab == null) {
            // We were unable to restore the tab. Let the app know so that it can workaround that
            onFailure(tab.url)
        }
        recoverableTab
    }

    if (tabs.isEmpty()) {
        return
    }

    invoke(tabs, selectTabId = tabs.firstOrNull()?.state?.id)

    if (!updateLastAccess) {
        return
    }

    val restoredTabIds = tabs.map { it.state.id }
    restoredTabIds.forEach { tabId ->
        store.dispatch(LastAccessAction.UpdateLastAccessAction(tabId))
    }
}
