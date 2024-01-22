/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations

import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.distinctUntilChangedBy
import kotlinx.coroutines.flow.mapNotNull
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.translate.initialFromLanguage
import mozilla.components.concept.engine.translate.initialToLanguage
import mozilla.components.lib.state.helpers.AbstractBinding

/**
 * Helper for observing Translation state from [BrowserStore].
 */
class TranslationsDialogBinding(
    browserStore: BrowserStore,
    private val translationsDialogStore: TranslationsDialogStore,
    private val sessionId: String,
    private val getTranslatedPageTitle: (localizedFrom: String?, localizedTo: String?) -> String,
) : AbstractBinding<BrowserState>(browserStore) {

    override suspend fun onState(flow: Flow<BrowserState>) {
        flow.mapNotNull { state -> state.findTab(sessionId) }
            .distinctUntilChangedBy {
                it.translationsState
            }
            .collect { sessionState ->
                val translationsState = sessionState.translationsState

                val fromSelected =
                    translationsState.translationEngineState?.initialFromLanguage(
                        translationsState.supportedLanguages?.fromLanguages,
                    )

                fromSelected?.let {
                    translationsDialogStore.dispatch(
                        TranslationsDialogAction.UpdateFromSelectedLanguage(
                            fromSelected,
                        ),
                    )
                }

                val toSelected =
                    translationsState.translationEngineState?.initialToLanguage(
                        translationsState.supportedLanguages?.toLanguages,
                    )
                toSelected?.let {
                    translationsDialogStore.dispatch(
                        TranslationsDialogAction.UpdateToSelectedLanguage(
                            toSelected,
                        ),
                    )
                }

                if (
                    toSelected != null && fromSelected != null &&
                    translationsDialogStore.state.translatedPageTitle == null
                ) {
                    translationsDialogStore.dispatch(
                        TranslationsDialogAction.UpdateTranslatedPageTitle(
                            getTranslatedPageTitle(
                                fromSelected.localizedDisplayName,
                                toSelected.localizedDisplayName,
                            ),
                        ),
                    )
                }

                val translateFromLanguages = translationsState.supportedLanguages?.fromLanguages
                translateFromLanguages?.let {
                    translationsDialogStore.dispatch(
                        TranslationsDialogAction.UpdateTranslateFromLanguages(
                            translateFromLanguages,
                        ),
                    )
                }

                val translateToLanguages = translationsState.supportedLanguages?.toLanguages
                translateToLanguages?.let {
                    translationsDialogStore.dispatch(
                        TranslationsDialogAction.UpdateTranslateToLanguages(
                            translateToLanguages,
                        ),
                    )
                }

                if (translationsState.isTranslateProcessing) {
                    updateStoreIfIsTranslateProcessing()
                }

                if (translationsState.isTranslated && !translationsState.isTranslateProcessing) {
                    updateStoreIfTranslated()
                }

                if (translationsState.translationError != null) {
                    translationsDialogStore.dispatch(
                        TranslationsDialogAction.UpdateTranslationError(translationsState.translationError),
                    )
                }
            }
    }

    private fun updateStoreIfIsTranslateProcessing() {
        translationsDialogStore.dispatch(
            TranslationsDialogAction.UpdateTranslationInProgress(
                true,
            ),
        )
        translationsDialogStore.dispatch(
            TranslationsDialogAction.DismissDialog(
                dismissDialogState = DismissDialogState.WaitingToBeDismissed,
            ),
        )
    }

    private fun updateStoreIfTranslated() {
        translationsDialogStore.dispatch(
            TranslationsDialogAction.UpdateTranslationInProgress(
                false,
            ),
        )

        translationsDialogStore.dispatch(
            TranslationsDialogAction.UpdateTranslated(
                true,
            ),
        )

        if (translationsDialogStore.state.dismissDialogState == DismissDialogState.WaitingToBeDismissed) {
            translationsDialogStore.dispatch(
                TranslationsDialogAction.DismissDialog(
                    dismissDialogState = DismissDialogState.Dismiss,
                ),
            )
        }
    }
}
