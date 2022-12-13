/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.locale.screen

import mozilla.components.lib.state.Action
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store

class LanguageScreenStore(
    initialState: LanguageScreenState,
    middlewares: List<Middleware<LanguageScreenState, LanguageScreenAction>> = emptyList(),
) : Store<LanguageScreenState, LanguageScreenAction>(
    initialState,
    ::languagesScreenStateReducer,
    middlewares,
) {
    init {
        dispatch(LanguageScreenAction.InitLanguages)
    }
}

/**
 * The state of the language selection screen
 *
 * @property languageList The full list of languages available
 * @property selectedLanguage The current selected language
 */
data class LanguageScreenState(
    val languageList: List<Language> = emptyList(),
    val selectedLanguage: Language? = null,
) : State

/**
 * Action to dispatch through the `LanguageScreenStore` to modify `LanguageScreenState` through the reducer.
 */
sealed class LanguageScreenAction : Action {
    object InitLanguages : LanguageScreenAction()
    data class Select(val selectedLanguage: Language) : LanguageScreenAction()
    data class UpdateLanguages(
        val languageList: List<Language>,
        val selectedLanguage: Language,
    ) : LanguageScreenAction()
}

/**
 * Reduces the language state from the current state and an action performed on it.
 *
 * @param state the current locale state
 * @param action the action to perform
 * @return the new locale state
 */
private fun languagesScreenStateReducer(
    state: LanguageScreenState,
    action: LanguageScreenAction,
): LanguageScreenState {
    return when (action) {
        is LanguageScreenAction.Select -> {
            state.copy(selectedLanguage = action.selectedLanguage)
        }
        is LanguageScreenAction.UpdateLanguages -> {
            state.copy(languageList = action.languageList, selectedLanguage = action.selectedLanguage)
        }
        LanguageScreenAction.InitLanguages -> {
            throw IllegalStateException("You need to add LanguageMiddleware to your LanguageScreenStore. ($action)")
        }
    }
}
