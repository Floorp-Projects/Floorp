/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations

import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.distinctUntilChangedBy
import kotlinx.coroutines.flow.mapNotNull
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.translate.initialFromLanguage
import mozilla.components.concept.engine.translate.initialToLanguage
import mozilla.components.lib.state.helpers.AbstractBinding

/**
 * Helper for observing Translation state from both [BrowserState.translationEngine]
 * and [TabSessionState.translationsState].
 */
class TranslationsDialogBinding(
    browserStore: BrowserStore,
    private val translationsDialogStore: TranslationsDialogStore,
    private val sessionId: String,
    private val getTranslatedPageTitle: (localizedFrom: String?, localizedTo: String?) -> String,
) : AbstractBinding<BrowserState>(browserStore) {

    @Suppress("LongMethod")
    override suspend fun onState(flow: Flow<BrowserState>) {
        // Browser level flows
        val browserFlow = flow.mapNotNull { state -> state }
            .distinctUntilChangedBy {
                it.translationEngine
            }

        // Session level flows
        val sessionFlow = flow.mapNotNull { state -> state.findTab(sessionId) }
            .distinctUntilChangedBy {
                it.translationsState
            }

        // Applying the flows together
        sessionFlow
            .combine(browserFlow) { sessionState, browserState -> TranslationsFlowState(sessionState, browserState) }
            .collect {
                    state ->
                // Browser Translations State Behavior (Global)
                val browserTranslationsState = state.browserState.translationEngine
                val translateFromLanguages =
                    browserTranslationsState.supportedLanguages?.fromLanguages
                translateFromLanguages?.let {
                    translationsDialogStore.dispatch(
                        TranslationsDialogAction.UpdateTranslateFromLanguages(
                            translateFromLanguages,
                        ),
                    )
                }

                val translateToLanguages =
                    browserTranslationsState.supportedLanguages?.toLanguages
                translateToLanguages?.let {
                    translationsDialogStore.dispatch(
                        TranslationsDialogAction.UpdateTranslateToLanguages(
                            translateToLanguages,
                        ),
                    )
                }

                // Dispatch engine level errors
                if (browserTranslationsState.engineError != null) {
                    translationsDialogStore.dispatch(
                        TranslationsDialogAction.UpdateTranslationError(browserTranslationsState.engineError),
                    )
                }

                // Session Translations State Behavior (Tab)
                val sessionTranslationsState = state.sessionState.translationsState
                val fromSelected =
                    sessionTranslationsState.translationEngineState?.initialFromLanguage(
                        translateFromLanguages,
                    )

                fromSelected?.let {
                    translationsDialogStore.dispatch(
                        TranslationsDialogAction.UpdateFromSelectedLanguage(
                            fromSelected,
                        ),
                    )
                }

                val toSelected =
                    sessionTranslationsState.translationEngineState?.initialToLanguage(
                        translateToLanguages,
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

                if (sessionTranslationsState.isTranslateProcessing) {
                    updateStoreIfIsTranslateProcessing()
                }

                if (sessionTranslationsState.isTranslated && !sessionTranslationsState.isTranslateProcessing) {
                    updateStoreIfTranslated()
                }

                // A session error may override a browser error
                if (sessionTranslationsState.translationError != null) {
                    translationsDialogStore.dispatch(
                        TranslationsDialogAction.UpdateTranslationError(sessionTranslationsState.translationError),
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
