/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.fenix.translations

import mozilla.components.browser.state.action.TranslationsAction
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.translate.Language
import mozilla.components.concept.engine.translate.TranslationError
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store

/**
 * The [TranslationsDialogStore] holds the [TranslationsDialogState] (state tree).
 *
 * The only way to change the [TranslationsDialogState] inside
 * [TranslationsDialogStore] is to dispatch an [Action] on it.
 */
class TranslationsDialogStore(
    initialState: TranslationsDialogState,
    middlewares: List<Middleware<TranslationsDialogState, TranslationsDialogAction>> = emptyList(),
) : Store<TranslationsDialogState, TranslationsDialogAction>(
    initialState,
    TranslationsDialogReducer::reduce,
    middlewares,
) {
    init {
        dispatch(TranslationsDialogAction.FetchSupportedLanguages)
    }
}

/**
 * The current state of the Translations bottom sheet dialog.
 *
 * @property isTranslated The page is currently translated.
 * @property isTranslationInProgress The page is currently attempting a translation.
 * @property positiveButtonType Can be enabled,disabled or in progress.
 * @property error An error that can occur during the translation process.
 * @property dismissDialogState Whether the dialog bottom sheet should be dismissed.
 * @property initialFrom Initial "from" language, based on the translation state and page state.
 * @property initialTo Initial "to" language, based on the translation state and page state.
 * @property fromLanguages Translation menu items to be shown in the translate from dropdown.
 * @property toLanguages Translation menu items to be shown in the translate to dropdown.
 * @property translatedPageTitle Title of the bottom sheet dialogue if the page was translated.
 */
data class TranslationsDialogState(
    var isTranslated: Boolean = false,
    val isTranslationInProgress: Boolean = false,
    val positiveButtonType: PositiveButtonType? = null,
    val error: TranslationError? = null,
    val dismissDialogState: DismissDialogState? = null,
    val initialFrom: Language? = null,
    val initialTo: Language? = null,
    val fromLanguages: List<Language>? = null,
    val toLanguages: List<Language>? = null,
    val translatedPageTitle: String? = null,
) : State

/**
 * Action to dispatch through the `TranslationsStore` to modify `TranslationsDialogState` through the reducer.
 */
sealed class TranslationsDialogAction : Action {
    /**
     * Invoked when the [TranslationsDialogStore] is added to the fragment.
     */
    object InitTranslationsDialog : TranslationsDialogAction()

    /**
     * When FetchSupportedLanguages is dispatched, an [TranslationsAction.OperationRequestedAction]
     * will be dispatched to the [BrowserStore]
     */
    object FetchSupportedLanguages : TranslationsDialogAction()

    /**
     * Invoked when the user wants to translate a website.
     */
    object TranslateAction : TranslationsDialogAction()

    /**
     * Invoked when the user wants to restore the website to its original pre-translated content.
     */
    object RestoreTranslation : TranslationsDialogAction()

    /**
     * Invoked when a translation error occurs during the translation process.
     */
    data class UpdateTranslationError(
        val translationError: TranslationError? = null,
    ) : TranslationsDialogAction()

    /**
     * Updates translate from languages list.
     */
    data class UpdateTranslateFromLanguages(
        val translateFromLanguages: List<Language>,
    ) : TranslationsDialogAction()

    /**
     * Updates translate to languages list.
     */
    data class UpdateTranslateToLanguages(
        val translateToLanguages: List<Language>,
    ) : TranslationsDialogAction()

    /**
     * Updates to the current selected language from the "translateFromLanguages" list.
     */
    data class UpdateFromSelectedLanguage(
        val language: Language,
    ) : TranslationsDialogAction()

    /**
     * Updates to the current selected language from the "translateToLanguages" list.
     */
    data class UpdateToSelectedLanguage(
        val language: Language,
    ) : TranslationsDialogAction()

    /**
     * Dismiss the translation dialog fragment.
     */
    data class DismissDialog(
        val dismissDialogState: DismissDialogState,
    ) : TranslationsDialogAction()

    /**
     * Updates the dialog content to translation in progress status.
     */
    data class UpdateTranslationInProgress(
        val inProgress: Boolean,
    ) : TranslationsDialogAction()

    /**
     * Updates the dialog content to translated status.
     */
    data class UpdateTranslated(
        val isTranslated: Boolean,
    ) : TranslationsDialogAction()

    /**
     * Updates the dialog title if the page was translated.
     */
    data class UpdateTranslatedPageTitle(val title: String) : TranslationsDialogAction()
}

/**
 * Positive button type from the translation bottom sheet.
 */
sealed class PositiveButtonType {
    /**
     * The translating indicator will appear.
     */
    object InProgress : PositiveButtonType()

    /**
     * The button is in a disabled state.
     */
    object Disabled : PositiveButtonType()

    /**
     * The button is in a enabled state.
     */
    object Enabled : PositiveButtonType()
}

/**
 * Dismiss translation bottom sheet type.
 */
sealed class DismissDialogState {
    /**
     * The dialog should be dismissed.
     */
    object Dismiss : DismissDialogState()

    /**
     * This is the step when translation is in progress and the dialog is waiting to be dismissed.
     */
    object WaitingToBeDismissed : DismissDialogState()
}

internal object TranslationsDialogReducer {
    /**
     * Reduces the translations dialog state from the current state and an action performed on it.
     *
     * @param state The current translations dialog state.
     * @param action The action to perform.
     * @return The new [TranslationsDialogState].
     */
    @Suppress("LongMethod")
    fun reduce(
        state: TranslationsDialogState,
        action: TranslationsDialogAction,
    ): TranslationsDialogState {
        return when (action) {
            is TranslationsDialogAction.UpdateFromSelectedLanguage -> {
                state.copy(
                    initialFrom = action.language,
                    positiveButtonType = if (state.initialTo != null && action.language != state.initialTo) {
                        PositiveButtonType.Enabled
                    } else {
                        PositiveButtonType.Disabled
                    },
                )
            }

            is TranslationsDialogAction.UpdateToSelectedLanguage -> {
                state.copy(
                    initialTo = action.language,
                    positiveButtonType = if (state.initialFrom != null && action.language != state.initialFrom) {
                        PositiveButtonType.Enabled
                    } else {
                        PositiveButtonType.Disabled
                    },
                )
            }

            is TranslationsDialogAction.UpdateTranslateToLanguages -> {
                state.copy(toLanguages = action.translateToLanguages)
            }

            is TranslationsDialogAction.UpdateTranslateFromLanguages -> {
                state.copy(fromLanguages = action.translateFromLanguages)
            }

            is TranslationsDialogAction.DismissDialog -> {
                state.copy(dismissDialogState = action.dismissDialogState)
            }

            is TranslationsDialogAction.UpdateTranslationInProgress -> {
                state.copy(
                    isTranslationInProgress = action.inProgress,
                    positiveButtonType = if (action.inProgress) {
                        PositiveButtonType.InProgress
                    } else {
                        state.positiveButtonType
                    },
                )
            }

            is TranslationsDialogAction.InitTranslationsDialog -> {
                state.copy(
                    positiveButtonType = if (state.initialTo == null || state.initialFrom == null) {
                        PositiveButtonType.Disabled
                    } else {
                        state.positiveButtonType
                    },
                )
            }

            is TranslationsDialogAction.UpdateTranslationError -> {
                state.copy(
                    error = action.translationError,
                    positiveButtonType = if (
                        action.translationError is TranslationError.LanguageNotSupportedError
                    ) {
                        PositiveButtonType.Disabled
                    } else {
                        PositiveButtonType.Enabled
                    },
                )
            }

            is TranslationsDialogAction.UpdateTranslated -> {
                state.copy(
                    isTranslated = action.isTranslated,
                    positiveButtonType = PositiveButtonType.Disabled,
                )
            }

            is TranslationsDialogAction.UpdateTranslatedPageTitle -> {
                state.copy(translatedPageTitle = action.title)
            }

            is TranslationsDialogAction.TranslateAction,
            TranslationsDialogAction.FetchSupportedLanguages,
            TranslationsDialogAction.RestoreTranslation,
            -> {
                // handled by [TranslationsDialogMiddleware]
                state
            }
        }
    }
}
