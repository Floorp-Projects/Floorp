/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.helper

import androidx.compose.runtime.Composable
import androidx.compose.runtime.State
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import mozilla.components.browser.state.selector.findCustomTab
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.Store
import mozilla.components.lib.state.ext.observeAsComposableState

/**
 * Helper for allowing a component consumer to specify which tab a component should target (e.g.
 * the selected tab, a specific pinned tab or a custom tab). Additional helper methods make it
 * easier to lookup the current state of the tab or observe changes.
 */
sealed class Target {
    /**
     * Looks up this target in the given [BrowserStore] and returns the matching [SessionState] if
     * available. Otherwise returns `null`.
     *
     * @param store to lookup this target in.
     */
    fun lookupIn(store: BrowserStore): SessionState? = lookupIn(store.state)

    /**
     * Looks up this target in the given [BrowserState] and returns the matching [SessionState] if
     * available. Otherwise returns `null`.
     *
     * @param state to lookup this target in.
     */
    abstract fun lookupIn(state: BrowserState): SessionState?

    /**
     * Observes this target and represents the mapped state (using [map]) via [State].
     *
     * Everytime the [Store] state changes and the result of the [observe] function changes for this
     * state, the returned [State] will be updated causing recomposition of every [State.value] usage.
     *
     * The [Store] observer will automatically be removed when this composable disposes or the current
     * [LifecycleOwner] moves to the [Lifecycle.State.DESTROYED] state.
     *
     * @param store that should get observed
     * @param observe function that maps a [SessionState] to the (sub) state that should get observed
     * for changes.
     */
    @Composable
    fun <R> observeAsComposableStateFrom(
        store: BrowserStore,
        observe: (SessionState?) -> R,
    ): State<SessionState?> {
        return store.observeAsComposableState(
            map = { state -> lookupIn(state) },
            observe = { state -> observe(lookupIn(state)) },
        )
    }

    /**
     * Targets the selected tab.
     */
    object SelectedTab : Target() {
        override fun lookupIn(state: BrowserState): SessionState? {
            return state.selectedTab
        }
    }

    /**
     * Targets a specific tab by its [tabId].
     *
     * @param tabId The ID of the tab to be targeted.
     */
    class Tab(val tabId: String) : Target() {
        override fun lookupIn(state: BrowserState): SessionState? {
            return state.findTab(tabId)
        }
    }

    /**
     * Targets a specific custom tab by its [customTabId].
     *
     * @param customTabId The ID of the custom tab to be targeted.
     */
    class CustomTab(val customTabId: String) : Target() {
        override fun lookupIn(state: BrowserState): SessionState? {
            return state.findCustomTab(customTabId)
        }
    }
}
