/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.state

import mozilla.components.lib.state.Reducer

/**
 * Reducer creating a new [AppState] for dispatched [AppAction]s.
 */
object AppReducer : Reducer<AppState, AppAction> {
    override fun invoke(state: AppState, action: AppAction): AppState {
        return when (action) {
            is AppAction.SelectionChanged -> selectionChanged(state, action)
            is AppAction.NoTabs -> noTabs(state)
            is AppAction.EditAction -> editAction(state, action)
            is AppAction.FinishEdit -> finishEditing(state, action)
            is AppAction.ShowTabs -> showTabs(state)
            is AppAction.HideTabs -> hideTabs(state)
            is AppAction.FinishFirstRun -> finishFirstRun(state, action)
        }
    }
}

/**
 * The currently selected tab has changed.
 */
private fun selectionChanged(state: AppState, action: AppAction.SelectionChanged): AppState {
    if (state.screen is Screen.FirstRun) {
        return state
    }

    return state.copy(
        screen = Screen.Browser(tabId = action.tabId, showTabs = false)
    )
}

/**
 * All tabs have been closed.
 */
private fun noTabs(state: AppState): AppState {
    if (state.screen is Screen.Home || state.screen is Screen.FirstRun) {
        return state
    }

    return state.copy(screen = Screen.Home)
}

/**
 * The user wants to edit the URL of a tab.
 */
private fun editAction(state: AppState, action: AppAction.EditAction): AppState {
    return state.copy(
        screen = Screen.EditUrl(
            action.tabId,
            action.x,
            action.y,
            action.width,
            action.height
        )
    )
}

/**
 * The user finished editing the URL.
 */
private fun finishEditing(state: AppState, action: AppAction.FinishEdit): AppState {
    return state.copy(
        screen = Screen.Browser(tabId = action.tabId, showTabs = false)
    )
}

/**
 * Show the tabs tray.
 */
private fun showTabs(state: AppState): AppState {
    return if (state.screen is Screen.Browser) {
        state.copy(screen = state.screen.copy(showTabs = true))
    } else {
        state
    }
}

/**
 * Hide the tabs tray.
 */
private fun hideTabs(state: AppState): AppState {
    return if (state.screen is Screen.Browser) {
        state.copy(screen = state.screen.copy(showTabs = false))
    } else {
        state
    }
}

/**
 * The user finished the first run onboarding.
 */
private fun finishFirstRun(state: AppState, action: AppAction.FinishFirstRun): AppState {
    return if (action.tabId != null) {
        state.copy(screen = Screen.Browser(action.tabId, showTabs = false))
    } else {
        state.copy(screen = Screen.Home)
    }
}
