/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.advanced

import android.content.Context
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import mozilla.components.support.locale.LocaleManager
import java.util.Locale

class LocaleSettingsStore(
    initialState: LocaleSettingsState,
) : Store<LocaleSettingsState, LocaleSettingsAction>(
    initialState,
    ::localeSettingsStateReducer,
)

/**
 * The state of the language selection page
 * @property localeList The full list of locales available
 * @property searchedLocaleList The list of locales starting with a search query
 * @property selectedLocale The current selected locale
 */
data class LocaleSettingsState(
    val localeList: List<Locale>,
    val searchedLocaleList: List<Locale>,
    val selectedLocale: Locale,
) : State

fun createInitialLocaleSettingsState(context: Context): LocaleSettingsState {
    val supportedLocales = LocaleManager.getSupportedLocales()

    return LocaleSettingsState(
        supportedLocales,
        supportedLocales,
        selectedLocale = LocaleManager.getSelectedLocale(context),
    )
}

/**
 * Actions to dispatch through the `LocaleSettingsStore` to modify `LocaleSettingsState` through the reducer.
 */
sealed class LocaleSettingsAction : Action {
    data class Select(val selectedItem: Locale) : LocaleSettingsAction()
    data class Search(val query: String) : LocaleSettingsAction()
}

/**
 * Reduces the locale state from the current state and an action performed on it.
 * @param state the current locale state
 * @param action the action to perform
 * @return the new locale state
 */
private fun localeSettingsStateReducer(
    state: LocaleSettingsState,
    action: LocaleSettingsAction,
): LocaleSettingsState {
    return when (action) {
        is LocaleSettingsAction.Select -> {
            state.copy(selectedLocale = action.selectedItem, searchedLocaleList = state.localeList)
        }
        is LocaleSettingsAction.Search -> {
            val searchedItems = state.localeList.filter {
                it.getDisplayLanguage(it).startsWith(action.query, ignoreCase = true) ||
                    it.displayLanguage.startsWith(action.query, ignoreCase = true) ||
                    it === state.localeList[0]
            }
            state.copy(searchedLocaleList = searchedItems)
        }
    }
}
