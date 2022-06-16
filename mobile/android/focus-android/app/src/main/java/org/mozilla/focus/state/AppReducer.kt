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
            is AppAction.HideTabs -> hideTabs(state)
            is AppAction.ShowFirstRun -> showFirstRun(state)
            is AppAction.FinishFirstRun -> finishFirstRun(state, action)
            is AppAction.Lock -> lock(state)
            is AppAction.Unlock -> unlock(state, action)
            is AppAction.OpenSettings -> openSettings(state, action)
            is AppAction.NavigateUp -> navigateUp(state, action)
            is AppAction.OpenTab -> openTab(state, action)
            is AppAction.TopSitesChange -> topSitesChanged(state, action)
            is AppAction.SitePermissionOptionChange -> sitePermissionOptionChanged(state, action)
            is AppAction.SecretSettingsStateChange -> secretSettingsStateChanged(
                state,
                action
            )
            is AppAction.ShowEraseTabsCfrChange -> showEraseTabsCfrChanged(state, action)
            is AppAction.ShowTrackingProtectionCfrChange -> showTrackingProtectionCfrChanged(state, action)
            is AppAction.OpenSitePermissionOptionsScreen -> openSitePermissionOptionsScreen(state, action)
            is AppAction.ShowHomeScreen -> showHomeScreen(state)
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
    if (state.screen is Screen.Home || state.screen is Screen.FirstRun || state.screen is Screen.Locked) {
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
 * Force showing the home screen.
 */
private fun showHomeScreen(state: AppState): AppState {
    return state.copy(screen = Screen.Home)
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

/**
 * The rules of site permissions autoplay has changed.
 */
private fun sitePermissionOptionChanged(state: AppState, action: AppAction.SitePermissionOptionChange): AppState {
    return state.copy(sitePermissionOptionChange = action.value)
}

/**
 * The state of secret settings has changed.
 */
private fun secretSettingsStateChanged(state: AppState, action: AppAction.SecretSettingsStateChange): AppState {
    return state.copy(secretSettingsEnabled = action.enabled)
}

/**
 * The state of erase tabs CFR changed
 */
private fun showEraseTabsCfrChanged(state: AppState, action: AppAction.ShowEraseTabsCfrChange): AppState {
    return state.copy(showEraseTabsCfr = action.value)
}

/**
 * The state of tracking protection CFR changed
 */
private fun showTrackingProtectionCfrChanged(
    state: AppState,
    action: AppAction.ShowTrackingProtectionCfrChange
): AppState {
    return state.copy(showTrackingProtectionCfrForTab = action.value)
}

private fun openSitePermissionOptionsScreen(
    state: AppState,
    action: AppAction.OpenSitePermissionOptionsScreen
): AppState {
    return state.copy(screen = Screen.SitePermissionOptionsScreen(sitePermission = action.sitePermission))
}

@Suppress("ComplexMethod", "ReturnCount")
private fun navigateUp(state: AppState, action: AppAction.NavigateUp): AppState {
    if (state.screen is Screen.Browser) {
        val screen = if (action.tabId != null) {
            Screen.Browser(tabId = action.tabId, showTabs = false)
        } else {
            Screen.Home
        }
        return state.copy(screen = screen)
    }

    if (state.screen is Screen.SitePermissionOptionsScreen) {
        return state.copy(screen = Screen.Settings(page = Screen.Settings.Page.SitePermissions))
    }

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
        Screen.Settings.Page.SitePermissions -> Screen.Settings(page = Screen.Settings.Page.Privacy)
        Screen.Settings.Page.Studies -> Screen.Settings(page = Screen.Settings.Page.Privacy)
        Screen.Settings.Page.SecretSettings -> Screen.Settings(page = Screen.Settings.Page.Advanced)

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
        Screen.Settings.Page.About -> Screen.Settings(page = Screen.Settings.Page.Mozilla)
        Screen.Settings.Page.Locale -> Screen.Settings(page = Screen.Settings.Page.General)
    }

    return state.copy(screen = screen)
}
