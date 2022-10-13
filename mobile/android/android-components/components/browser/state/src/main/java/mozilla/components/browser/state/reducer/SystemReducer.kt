/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import android.content.ComponentCallbacks2
import mozilla.components.browser.state.action.SystemAction
import mozilla.components.browser.state.state.BrowserState

internal object SystemReducer {
    /**
     * [SystemAction] Reducer function for modifying [BrowserState].
     */
    fun reduce(state: BrowserState, action: SystemAction): BrowserState {
        return when (action) {
            is SystemAction.LowMemoryAction -> trimMemory(state, action.level)
        }
    }

    private fun trimMemory(state: BrowserState, level: Int): BrowserState {
        // We only take care of thumbnails here. EngineMiddleware deals with suspending
        // EngineSessions if needed.

        if (!shouldClearThumbnails(level)) {
            return state
        }

        return state.copy(
            tabs = state.tabs.map { tab ->
                if (tab.id != state.selectedTabId) {
                    tab.copy(
                        content = tab.content.copy(thumbnail = null),
                    )
                } else {
                    tab
                }
            },
            customTabs = state.customTabs.map { tab ->
                tab.copy(
                    content = tab.content.copy(thumbnail = null),
                )
            },
        )
    }
}

private fun shouldClearThumbnails(level: Int): Boolean {
    return when (level) {
        // Foreground: The device is running much lower on memory. The app is running and not killable, but the
        // system wants us to release unused resources to improve system performance.
        ComponentCallbacks2.TRIM_MEMORY_RUNNING_LOW,
        // Foreground: The device is running extremely low on memory. The app is not yet considered a killable
        // process, but the system will begin killing background processes if apps do not release resources.
        ComponentCallbacks2.TRIM_MEMORY_RUNNING_CRITICAL,
        -> true

        // Background: The system is running low on memory and our process is near the middle of the LRU list.
        // If the system becomes further constrained for memory, there's a chance our process will be killed.
        ComponentCallbacks2.TRIM_MEMORY_MODERATE,
        // Background: The system is running low on memory and our process is one of the first to be killed
        // if the system does not recover memory now.
        ComponentCallbacks2.TRIM_MEMORY_COMPLETE,
        -> true

        else -> false
    }
}
