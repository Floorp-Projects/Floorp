/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.TranslationsAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TranslationsState
import mozilla.components.concept.engine.translate.TranslationOperation

internal object TranslationsStateReducer {

    fun reduce(state: BrowserState, action: TranslationsAction): BrowserState = when (action) {
        is TranslationsAction.TranslateExpectedAction -> {
            state.copyWithTranslationsState(action.tabId) {
                it.copy(
                    isExpectedTranslate = true,
                )
            }
        }

        is TranslationsAction.TranslateOfferAction -> {
            state.copyWithTranslationsState(action.tabId) {
                it.copy(
                    isOfferTranslate = true,
                )
            }
        }

        is TranslationsAction.TranslateStateChangeAction -> {
            if (action.translationEngineState.requestedTranslationPair != null) {
                state.copyWithTranslationsState(action.tabId) {
                    it.copy(
                        isTranslated = true,
                    )
                }
            }
            state.copyWithTranslationsState(action.tabId) {
                it.copy(
                    translationEngineState = action.translationEngineState,
                )
            }
        }

        is TranslationsAction.TranslateAction ->
            state.copyWithTranslationsState(action.tabId) {
                it.copy(isTranslateProcessing = true)
            }

        is TranslationsAction.TranslateRestoreAction ->
            state.copyWithTranslationsState(action.tabId) {
                it.copy(isRestoreProcessing = true)
            }

        is TranslationsAction.TranslateSuccessAction -> {
            if (TranslationOperation.TRANSLATE == action.operation) {
                state.copyWithTranslationsState(action.tabId) {
                    it.copy(
                        isTranslated = true,
                        isTranslateProcessing = false,
                    )
                }
            } else {
                // Restore
                state.copyWithTranslationsState(action.tabId) {
                    it.copy(
                        isTranslated = false,
                        isRestoreProcessing = false,
                    )
                }
            }
        }

        is TranslationsAction.TranslateExceptionAction -> {
            if (TranslationOperation.TRANSLATE == action.operation) {
                state.copyWithTranslationsState(action.tabId) {
                    it.copy(
                        isTranslateProcessing = false,
                    )
                }
            } else {
                // Restore
                state.copyWithTranslationsState(action.tabId) {
                    it.copy(
                        isRestoreProcessing = false,
                    )
                }
            }
        }
    }

    private inline fun BrowserState.copyWithTranslationsState(
        tabId: String,
        crossinline update: (TranslationsState) -> TranslationsState,
    ): BrowserState {
        return updateTabOrCustomTabState(tabId) { current ->
            current.createCopy(translationsState = update(current.translationsState))
        }
    }
}
