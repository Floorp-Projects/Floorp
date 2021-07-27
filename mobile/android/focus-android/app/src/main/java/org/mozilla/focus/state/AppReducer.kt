/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.focus.state

import mozilla.components.feature.top.sites.TopSite
import mozilla.components.lib.state.Reducer

/**
 * Reducer creating a new [AppState] for dispatched [AppAction]s.
 */
@Suppress("ComplexMethod")
object AppReducer : Reducer<AppState, AppAction> {
    override fun invoke(state: AppState, action: AppAction): AppState {
        return when (action) {
            is AppAction.SelectionChanged -> selectionChanged(state, action)
            is AppAction.NoTabs -> noTabs(state)
            is AppAction.EditAction -> editAction(state, action)
            is AppAction.FinishEdit -> finishEditing(state, action)
            is AppAction.ShowTabs -> showTabs(state)
            is AppAction.HideTabs -> hideTabs(state)
            is AppAction.ShowFirstRun -> showFirstRun(state)
            is AppAction.FinishFirstRun -> finishFirstRun(state, action)
            is AppAction.Lock -> lock(state)
            is AppAction.Unlock -> unlock(state, action)
            is AppAction.OpenSettings -> openSettings(state, action)
            is AppAction.NavigateUp -> navigateUp(state, action)
            is AppAction.OpenTab -> openTab(state, action)
            is AppAction.TopSitesChange -> topSitesChanged(state, action)
        }
    }
}

/**
 * The currently selected tab has changed.
 */
private fun selectionChanged(state: AppState, action: AppAction.SelectionChanged): AppState {
    if (state.screen is Screen.FirstRun || state.screen is Screen.Locked) {
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
        screen = Screen.EditUrl(action.tabId)
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

/**
 * Force showing the first run screen (for testing).
 */
private fun showFirstRun(state: AppState): AppState {
    return state.copy(screen = Screen.FirstRun)
}

/**
 * Lock the application.
 */
private fun lock(state: AppState): AppState {
    return state.copy(screen = Screen.Locked)
}

/**
 * Unlock the application.
 */
private fun unlock(state: AppState, action: AppAction.Unlock): AppState {
    if (state.screen !is Screen.Locked) {
        return state
    }

    return if (action.tabId != null) {
        state.copy(screen = Screen.Browser(action.tabId, showTabs = false))
    } else {
        state.copy(screen = Screen.Home)
    }
}

private fun openSettings(state: AppState, action: AppAction.OpenSettings): AppState {
    return state.copy(
        screen = Screen.Settings(page = action.page)
    )
}

private fun openTab(state: AppState, action: AppAction.OpenTab): AppState {
    return state.copy(
        screen = Screen.Browser(tabId = action.tabId, showTabs = false)
    )
}

/**
 * The list of [TopSite] has changed.
 */
private fun topSitesChanged(state: AppState, action: AppAction.TopSitesChange): AppState {
    return state.copy(topSites = action.topSites)
}

@Suppress("ComplexMethod")
private fun navigateUp(state: AppState, action: AppAction.NavigateUp): AppState {
    if (state.screen !is Screen.Settings) {
        return state
    }

    val screen = when (state.screen.page) {
        Screen.Settings.Page.Start -> if (action.tabId != null) {
            Screen.Browser(tabId = action.tabId, showTabs = false)
        } else {
            Screen.Home
        }

        Screen.Settings.Page.General -> Screen.Settings(page = Screen.Settings.Page.Start)
        Screen.Settings.Page.Privacy -> Screen.Settings(page = Screen.Settings.Page.Start)
        Screen.Settings.Page.Search -> Screen.Settings(page = Screen.Settings.Page.Start)
        Screen.Settings.Page.Advanced -> Screen.Settings(page = Screen.Settings.Page.Start)
        Screen.Settings.Page.Mozilla -> Screen.Settings(page = Screen.Settings.Page.Start)

        Screen.Settings.Page.PrivacyExceptions -> Screen.Settings(page = Screen.Settings.Page.Privacy)
        Screen.Settings.Page.PrivacyExceptionsRemove -> Screen.Settings(page = Screen.Settings.Page.PrivacyExceptions)

        Screen.Settings.Page.SearchList -> Screen.Settings(page = Screen.Settings.Page.Search)
        Screen.Settings.Page.SearchRemove -> Screen.Settings(page = Screen.Settings.Page.SearchList)
        Screen.Settings.Page.SearchAdd -> Screen.Settings(page = Screen.Settings.Page.SearchList)
        Screen.Settings.Page.SearchAutocomplete -> Screen.Settings(page = Screen.Settings.Page.Search)
        Screen.Settings.Page.SearchAutocompleteList -> Screen.Settings(page = Screen.Settings.Page.SearchAutocomplete)

        Screen.Settings.Page.SearchAutocompleteAdd -> Screen.Settings(
            page = Screen.Settings.Page.SearchAutocompleteList
        )
        Screen.Settings.Page.SearchAutocompleteRemove -> Screen.Settings(
            page = Screen.Settings.Page.SearchAutocompleteList
        )
    }

    return state.copy(screen = screen)
}
