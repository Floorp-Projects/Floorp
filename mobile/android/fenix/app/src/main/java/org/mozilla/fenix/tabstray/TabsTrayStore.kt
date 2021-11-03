/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store

/**
 * Value type that represents the state of the tabs tray.
 *
 * @property selectedPage The current page in the tray can be on.
 * @property mode Whether the browser tab list is in multi-select mode or not with the set of
 * currently selected tabs.
 * @property syncing Whether the Synced Tabs feature should fetch the latest tabs from paired
 * devices.
 * @property focusGroupTabId The search group tab id to focus. Defaults to null.
 */
data class TabsTrayState(
    val selectedPage: Page = Page.NormalTabs,
    val mode: Mode = Mode.Normal,
    val syncing: Boolean = false,
    val focusGroupTabId: String? = null
) : State {

    /**
     * The current mode that the tabs list is in.
     */
    sealed class Mode {

        /**
         * A set of selected tabs which we would want to perform an action on.
         */
        open val selectedTabs = emptySet<TabSessionState>()

        /**
         * The default mode the tabs list is in.
         */
        object Normal : Mode()

        /**
         * The multi-select mode that the tabs list is in containing the set of currently
         * selected tabs.
         */
        data class Select(override val selectedTabs: Set<TabSessionState>) : Mode()
    }
}

/**
 * The different pagers in the tray that we can switch between in the [TrayPagerAdapter].
 */
enum class Page {

    /**
     * The pager position that displays normal tabs.
     */
    NormalTabs,

    /**
     * The pager position that displays private tabs.
     */
    PrivateTabs,

    /**
     * The pager position that displays Synced Tabs.
     */
    SyncedTabs;

    companion object {
        fun positionToPage(position: Int): Page {
            return when (position) {
                0 -> NormalTabs
                1 -> PrivateTabs
                else -> SyncedTabs
            }
        }
    }
}

/**
 * [Action] implementation related to [TabsTrayStore].
 */
sealed class TabsTrayAction : Action {

    /**
     * Entered multi-select mode.
     */
    object EnterSelectMode : TabsTrayAction()

    /**
     * Exited multi-select mode.
     */
    object ExitSelectMode : TabsTrayAction()

    /**
     * Added a new [TabSessionState] to the selection set.
     */
    data class AddSelectTab(val tab: TabSessionState) : TabsTrayAction()

    /**
     * Removed a [TabSessionState] from the selection set.
     */
    data class RemoveSelectTab(val tab: TabSessionState) : TabsTrayAction()

    /**
     * The active page in the tray that is now in focus.
     */
    data class PageSelected(val page: Page) : TabsTrayAction()

    /**
     * A request to perform a "sync" action.
     */
    object SyncNow : TabsTrayAction()

    /**
     * When a "sync" action has completed; this can be triggered immediately after [SyncNow] if
     * no sync action was able to be performed.
     */
    object SyncCompleted : TabsTrayAction()

    /**
     * Removes the [TabsTrayState.focusGroupTabId] of the [TabsTrayState].
     */
    object ConsumeFocusGroupTabIdAction : TabsTrayAction()
}

/**
 * Reducer for [TabsTrayStore].
 */
internal object TabsTrayReducer {
    fun reduce(state: TabsTrayState, action: TabsTrayAction): TabsTrayState {
        return when (action) {
            is TabsTrayAction.EnterSelectMode ->
                state.copy(mode = TabsTrayState.Mode.Select(emptySet()))
            is TabsTrayAction.ExitSelectMode ->
                state.copy(mode = TabsTrayState.Mode.Normal)
            is TabsTrayAction.AddSelectTab ->
                state.copy(mode = TabsTrayState.Mode.Select(state.mode.selectedTabs + action.tab))
            is TabsTrayAction.RemoveSelectTab -> {
                val selected = state.mode.selectedTabs - action.tab
                state.copy(
                    mode = if (selected.isEmpty()) {
                        TabsTrayState.Mode.Normal
                    } else {
                        TabsTrayState.Mode.Select(selected)
                    }
                )
            }
            is TabsTrayAction.PageSelected ->
                state.copy(selectedPage = action.page)
            is TabsTrayAction.SyncNow ->
                state.copy(syncing = true)
            is TabsTrayAction.SyncCompleted ->
                state.copy(syncing = false)
            is TabsTrayAction.ConsumeFocusGroupTabIdAction ->
                state.copy(focusGroupTabId = null)
        }
    }
}

/**
 * A [Store] that holds the [TabsTrayState] for the tabs tray and reduces [TabsTrayAction]s
 * dispatched to the store.
 */
class TabsTrayStore(
    initialState: TabsTrayState = TabsTrayState(),
    middlewares: List<Middleware<TabsTrayState, TabsTrayAction>> = emptyList()
) : Store<TabsTrayState, TabsTrayAction>(
    initialState,
    TabsTrayReducer::reduce,
    middlewares
)
