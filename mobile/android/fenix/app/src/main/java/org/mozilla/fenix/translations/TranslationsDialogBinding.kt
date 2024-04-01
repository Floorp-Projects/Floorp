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
import mozilla.components.browser.state.state.TranslationsBrowserState
import mozilla.components.browser.state.state.TranslationsState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.translate.TranslationError
import mozilla.components.concept.engine.translate.initialFromLanguage
import mozilla.components.concept.engine.translate.initialToLanguage
import mozilla.components.lib.state.helpers.AbstractBinding
import mozilla.components.support.locale.LocaleManager
import org.mozilla.fenix.utils.LocaleUtils
import java.util.Locale

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

    @Suppress("LongMethod", "CyclomaticComplexMethod")
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

                // Session Translations State Behavior (Tab)
                val sessionTranslationsState = state.sessionState.translationsState

                val fromSelected =
                    sessionTranslationsState.translationEngineState?.initialFromLanguage(
                        translateFromLanguages,
                    )

                // Dispatch initialFrom Language only the first time when it is null.
                if (fromSelected != null && translationsDialogStore.state.initialFrom == null) {
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

                // Dispatch initialTo Language only the first time when it is null.
                if (toSelected != null && translationsDialogStore.state.initialTo == null) {
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

                sessionTranslationsState.translationDownloadSize?.let {
                    translationsDialogStore.dispatch(
                        TranslationsDialogAction.UpdateDownloadTranslationDownloadSize(it),
                    )
                }

                // Error handling requires both knowledge of browser and session state
                updateTranslationError(
                    sessionTranslationsState = sessionTranslationsState,
                    browserTranslationsState = browserTranslationsState,
                    browserState = state.browserState,
                )
            }
    }

    /**
     * Helper function for error handling, which requires considering both
     * [TranslationsState] (session) and [TranslationsBrowserState] (browser or global) states.
     *
     * Will dispatch [TranslationsDialogAction] to update store when appropriate.
     *
     * Certain errors take priority over others, depending on how important they are.
     * Error priority:
     * 1. An error in [TranslationsBrowserState] of [EngineNotSupportedError] should be the
     * highest priority to process.
     * 2. An error in [TranslationsState] of [LanguageNotSupportedError] is the second highest
     * priority and requires additional information.
     * 3. Displayable browser errors only should be the dialog error when the session has a
     * non-displayable error. (Usually expect this case where an error  recovery action occurred on
     * a different tab and failed.)
     * 4. Displayable session errors and null errors are the default dialog error.
     *
     * @param sessionTranslationsState The session state to consider when dispatching errors.
     * @param browserTranslationsState The browser state to consider when dispatching errors.
     * @param browserState The browser state to consider when fetching information for errors.

     */
    private fun updateTranslationError(
        sessionTranslationsState: TranslationsState,
        browserTranslationsState: TranslationsBrowserState,
        browserState: BrowserState,
    ) {
        if (browserTranslationsState.engineError is TranslationError.EngineNotSupportedError) {
            // [EngineNotSupportedError] is a browser error that overrides all other errors.
            translationsDialogStore.dispatch(
                TranslationsDialogAction.UpdateTranslationError(browserTranslationsState.engineError),
            )
        } else if (sessionTranslationsState.translationError is TranslationError.LanguageNotSupportedError) {
            // [LanguageNotSupportedError] is a special case where we need additional information.
            val documentLangDisplayName = sessionTranslationsState.translationEngineState
                ?.detectedLanguages?.documentLangTag?.let { docLangTag ->
                    val documentLocale = Locale.forLanguageTag(docLangTag)
                    val userLocale = browserState.locale ?: LocaleManager.getSystemDefault()
                    LocaleUtils.getLocalizedDisplayName(
                        userLocale = userLocale,
                        languageLocale = documentLocale,
                    )
                }

            translationsDialogStore.dispatch(
                TranslationsDialogAction.UpdateTranslationError(
                    translationError = sessionTranslationsState.translationError,
                    documentLangDisplayName = documentLangDisplayName,
                ),
            )
        } else if (browserTranslationsState.engineError?.displayError == true &&
            sessionTranslationsState.translationError?.displayError != true
        ) {
            // Browser errors should only be displayed with the session error is not displayable nor null.
            translationsDialogStore.dispatch(
                TranslationsDialogAction.UpdateTranslationError(browserTranslationsState.engineError),
            )
        } else if (sessionTranslationsState.translationError?.displayError != false) {
            // Displayable session errors and null session errors should be passed to the dialog under most cases.
            translationsDialogStore.dispatch(
                TranslationsDialogAction.UpdateTranslationError(sessionTranslationsState.translationError),
            )
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

        if (!translationsDialogStore.state.isTranslated) {
            translationsDialogStore.dispatch(
                TranslationsDialogAction.UpdateTranslated(
                    true,
                ),
            )
        }

        if (translationsDialogStore.state.dismissDialogState == DismissDialogState.WaitingToBeDismissed) {
            translationsDialogStore.dispatch(
                TranslationsDialogAction.DismissDialog(
                    dismissDialogState = DismissDialogState.Dismiss,
                ),
            )
        }
    }
}
