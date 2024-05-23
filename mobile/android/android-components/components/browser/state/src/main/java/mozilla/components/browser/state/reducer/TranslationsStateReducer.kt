/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.TranslationsAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.TranslationsState
import mozilla.components.concept.engine.translate.LanguageModel
import mozilla.components.concept.engine.translate.ModelOperation
import mozilla.components.concept.engine.translate.ModelState
import mozilla.components.concept.engine.translate.TranslationError
import mozilla.components.concept.engine.translate.TranslationOperation
import mozilla.components.concept.engine.translate.TranslationPageSettingOperation
import mozilla.components.concept.engine.translate.TranslationPageSettings

internal object TranslationsStateReducer {

    /**
     * Reducer for [BrowserState.translationEngine] and [SessionState.translationsState]
     */
    @Suppress("LongMethod")
    fun reduce(state: BrowserState, action: TranslationsAction): BrowserState = when (action) {
        TranslationsAction.InitTranslationsBrowserState -> {
            // No state change on this operation
            state
        }

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
                    isOfferTranslate = action.isOfferTranslate,
                )
            }
        }

        is TranslationsAction.TranslateStateChangeAction -> {
            var isExpectedTranslate = state.findTab(action.tabId)?.translationsState?.isExpectedTranslate ?: true
            var isOfferTranslate = state.findTab(action.tabId)?.translationsState?.isOfferTranslate ?: true

            // Checking if a translation can be anticipated or not based on
            // the new translation engine state detected metadata.
            if (action.translationEngineState.detectedLanguages == null ||
                action.translationEngineState.detectedLanguages?.supportedDocumentLang == false ||
                action.translationEngineState.detectedLanguages?.userPreferredLangTag == null
            ) {
                // Value can also update through [TranslateExpectedAction]
                // via the translations engine.
                isExpectedTranslate = false

                // Value can also update through [TranslateOfferAction]
                // via the translations engine.
                isOfferTranslate = false
            }

            // Checking for if the translations engine is in the fully translated state or not based
            // on if a visual change has occurred on the browser.
            if (action.translationEngineState.hasVisibleChange != true) {
                // In an untranslated state
                var translationsError: TranslationError? = null
                if (action.translationEngineState.detectedLanguages?.supportedDocumentLang == false) {
                    translationsError = TranslationError.LanguageNotSupportedError(cause = null)
                }
                state.copyWithTranslationsState(action.tabId) {
                    it.copy(
                        isOfferTranslate = isOfferTranslate,
                        isExpectedTranslate = isExpectedTranslate,
                        isTranslated = false,
                        translationEngineState = action.translationEngineState,
                        translationError = translationsError,
                    )
                }
            } else {
                // In a translated state
                state.copyWithTranslationsState(action.tabId) {
                    it.copy(
                        isOfferTranslate = isOfferTranslate,
                        isExpectedTranslate = isExpectedTranslate,
                        isTranslated = true,
                        translationError = null,
                        translationEngineState = action.translationEngineState,
                    )
                }
            }
        }

        is TranslationsAction.TranslateAction ->
            state.copyWithTranslationsState(action.tabId) {
                it.copy(
                    isOfferTranslate = false,
                    isTranslateProcessing = true,
                )
            }

        is TranslationsAction.TranslateRestoreAction ->
            state.copyWithTranslationsState(action.tabId) {
                it.copy(isRestoreProcessing = true)
            }

        is TranslationsAction.TranslateSuccessAction -> {
            when (action.operation) {
                TranslationOperation.TRANSLATE -> {
                    // The isTranslated state will be identified on a translation state change.
                    state.copyWithTranslationsState(action.tabId) {
                        it.copy(
                            isTranslateProcessing = false,
                            translationError = null,
                        )
                    }
                }

                TranslationOperation.RESTORE -> {
                    state.copyWithTranslationsState(action.tabId) {
                        it.copy(
                            isTranslated = false,
                            isRestoreProcessing = false,
                            translationError = null,
                        )
                    }
                }

                TranslationOperation.FETCH_SUPPORTED_LANGUAGES -> {
                    // Reset the error state, and then generally expect
                    // [TranslationsAction.SetSupportedLanguagesAction] to update state in the
                    // success case.
                    state.copyWithTranslationsState(action.tabId) {
                        it.copy(
                            translationError = null,
                        )
                    }
                }

                TranslationOperation.FETCH_LANGUAGE_MODELS -> {
                    // Reset the error state, and then generally expect
                    // [TranslationsAction.SetLanguageModelsAction] to update state in the
                    // success case.
                    state.copyWithTranslationsState(action.tabId) {
                        it.copy(
                            translationError = null,
                        )
                    }
                }

                TranslationOperation.FETCH_PAGE_SETTINGS -> {
                    // Reset the error state, and then generally expect
                    // [TranslationsAction.SetPageSettingsAction] to update state in the
                    // success case.
                    state.copyWithTranslationsState(action.tabId) {
                        it.copy(
                            settingsError = null,
                        )
                    }
                }

                TranslationOperation.FETCH_OFFER_SETTING -> {
                    // Reset the error state, and then generally expect
                    // [TranslationsAction.SetGlobalOfferTranslateSettingAction] to update state in the
                    // success case.
                    state.copyWithTranslationsState(action.tabId) {
                        it.copy(
                            settingsError = null,
                        )
                    }
                }

                TranslationOperation.FETCH_AUTOMATIC_LANGUAGE_SETTINGS -> {
                    state.copy(
                        translationEngine = state.translationEngine.copy(
                            engineError = null,
                        ),
                    )
                }

                TranslationOperation.FETCH_NEVER_TRANSLATE_SITES -> {
                    // Reset the error state, and then generally expect
                    // [TranslationsAction.SetNeverTranslateSitesAction] to update
                    // state in the success case.
                    state.copy(
                        translationEngine = state.translationEngine.copy(
                            neverTranslateSites = null,
                        ),
                    )
                }
            }
        }

        is TranslationsAction.TranslateExceptionAction -> {
            when (action.operation) {
                TranslationOperation.TRANSLATE -> {
                    state.copyWithTranslationsState(action.tabId) {
                        it.copy(
                            isTranslateProcessing = false,
                            translationError = action.translationError,
                        )
                    }
                }

                TranslationOperation.RESTORE -> {
                    state.copyWithTranslationsState(action.tabId) {
                        it.copy(
                            isRestoreProcessing = false,
                            translationError = action.translationError,
                        )
                    }
                }

                TranslationOperation.FETCH_SUPPORTED_LANGUAGES -> {
                    state.copyWithTranslationsState(action.tabId) {
                        it.copy(
                            translationError = action.translationError,
                        )
                    }
                }

                TranslationOperation.FETCH_LANGUAGE_MODELS -> {
                    state.copyWithTranslationsState(action.tabId) {
                        it.copy(
                            translationError = action.translationError,
                        )
                    }
                }

                TranslationOperation.FETCH_PAGE_SETTINGS -> {
                    state.copyWithTranslationsState(action.tabId) {
                        it.copy(
                            pageSettings = null,
                            settingsError = action.translationError,
                        )
                    }
                }

                TranslationOperation.FETCH_OFFER_SETTING -> {
                    state.copyWithTranslationsState(action.tabId) {
                        it.copy(
                            translationError = action.translationError,
                        )
                    }
                }

                TranslationOperation.FETCH_AUTOMATIC_LANGUAGE_SETTINGS -> {
                    state.copyWithTranslationsState(action.tabId) {
                        it.copy(
                            translationError = action.translationError,
                        )
                    }
                }

                TranslationOperation.FETCH_NEVER_TRANSLATE_SITES -> {
                    state.copyWithTranslationsState(action.tabId) {
                        it.copy(
                            settingsError = action.translationError,
                        )
                    }
                }
            }
        }

        is TranslationsAction.EngineExceptionAction -> {
            state.copy(translationEngine = state.translationEngine.copy(engineError = action.error))
        }

        is TranslationsAction.SetSupportedLanguagesAction ->
            state.copy(
                translationEngine = state.translationEngine.copy(
                    supportedLanguages = action.supportedLanguages,
                    engineError = null,
                ),
            )

        is TranslationsAction.SetLanguageModelsAction ->
            state.copy(
                translationEngine = state.translationEngine.copy(
                    languageModels = action.languageModels,
                    engineError = null,
                ),
            )

        is TranslationsAction.SetPageSettingsAction ->
            state.copyWithTranslationsState(action.tabId) {
                it.copy(
                    pageSettings = action.pageSettings,
                    settingsError = null,
                )
            }

        is TranslationsAction.SetNeverTranslateSitesAction ->
            state.copy(
                translationEngine = state.translationEngine.copy(
                    neverTranslateSites = action.neverTranslateSites,
                ),
            )

        is TranslationsAction.RemoveNeverTranslateSiteAction -> {
            val neverTranslateSites = state.translationEngine.neverTranslateSites
            val updatedNeverTranslateSites = neverTranslateSites?.filter { it != action.origin }?.toList()
            state.copy(
                translationEngine = state.translationEngine.copy(
                    neverTranslateSites = updatedNeverTranslateSites,
                ),
            )
        }

        is TranslationsAction.OperationRequestedAction ->
            when (action.operation) {
                TranslationOperation.FETCH_SUPPORTED_LANGUAGES -> {
                    state.copy(
                        translationEngine = state.translationEngine.copy(
                            supportedLanguages = null,
                        ),
                    )
                }
                TranslationOperation.FETCH_LANGUAGE_MODELS -> {
                    state.copy(
                        translationEngine = state.translationEngine.copy(
                            languageModels = null,
                        ),
                    )
                }

                TranslationOperation.FETCH_AUTOMATIC_LANGUAGE_SETTINGS -> {
                    state.copy(
                        translationEngine = state.translationEngine.copy(
                            languageSettings = null,
                        ),
                    )
                }

                TranslationOperation.FETCH_PAGE_SETTINGS -> {
                    state.copyWithTranslationsState(action.tabId) {
                        it.copy(
                            pageSettings = null,
                        )
                    }
                }

                TranslationOperation.FETCH_OFFER_SETTING -> {
                    state.copy(
                        translationEngine = state.translationEngine.copy(
                            offerTranslation = null,
                        ),
                    )
                }

                TranslationOperation.FETCH_NEVER_TRANSLATE_SITES -> {
                    state.copy(
                        translationEngine = state.translationEngine.copy(
                            neverTranslateSites = null,
                        ),
                    )
                }
                TranslationOperation.TRANSLATE, TranslationOperation.RESTORE -> {
                    // No state change for these operations
                    state
                }
            }

        is TranslationsAction.UpdatePageSettingAction -> {
            val currentPageSettings =
                state.findTab(action.tabId)?.translationsState?.pageSettings ?: TranslationPageSettings()

            when (action.operation) {
                TranslationPageSettingOperation.UPDATE_ALWAYS_OFFER_POPUP -> {
                    state.copyWithTranslationsState(action.tabId) {
                        it.copy(
                            pageSettings = currentPageSettings.copy(alwaysOfferPopup = action.setting),
                        )
                    }
                }

                TranslationPageSettingOperation.UPDATE_ALWAYS_TRANSLATE_LANGUAGE -> {
                    val alwaysTranslateLang = action.setting
                    var neverTranslateLang = currentPageSettings.neverTranslateLanguage

                    if (alwaysTranslateLang) {
                        // Always and never translate sites are always opposites when the other is true.
                        neverTranslateLang = false
                    }

                    state.copyWithTranslationsState(action.tabId) {
                        it.copy(
                            pageSettings = currentPageSettings.copy(
                                alwaysTranslateLanguage = alwaysTranslateLang,
                                neverTranslateLanguage = neverTranslateLang,
                            ),
                        )
                    }
                }

                TranslationPageSettingOperation.UPDATE_NEVER_TRANSLATE_LANGUAGE -> {
                    var alwaysTranslateLang = currentPageSettings.alwaysTranslateLanguage
                    val neverTranslateLang = action.setting

                    if (neverTranslateLang) {
                        // Always and never translate sites are always opposites when the other is true.
                        alwaysTranslateLang = false
                    }

                    state.copyWithTranslationsState(action.tabId) {
                        it.copy(
                            pageSettings = currentPageSettings.copy(
                                alwaysTranslateLanguage = alwaysTranslateLang,
                                neverTranslateLanguage = neverTranslateLang,
                            ),
                        )
                    }
                }

                TranslationPageSettingOperation.UPDATE_NEVER_TRANSLATE_SITE -> {
                    state.copyWithTranslationsState(action.tabId) {
                        it.copy(
                            pageSettings = currentPageSettings.copy(neverTranslateSite = action.setting),
                        )
                    }
                }
            }
        }

        is TranslationsAction.UpdateLanguageSettingsAction -> {
            val languageSettings = state.translationEngine.languageSettings?.toMutableMap()
            // Only set when keys are present.
            if (languageSettings?.get(action.languageCode) != null) {
                languageSettings[action.languageCode] = action.setting
            }
            state.copy(
                translationEngine = state.translationEngine.copy(
                    languageSettings = languageSettings,
                ),
            )
        }

        is TranslationsAction.SetGlobalOfferTranslateSettingAction -> {
            state.copy(
                translationEngine = state.translationEngine.copy(
                    offerTranslation = action.offerTranslation,
                ),
            )
        }

        is TranslationsAction.UpdateGlobalOfferTranslateSettingAction -> {
            state.copy(
                translationEngine = state.translationEngine.copy(
                    offerTranslation = action.offerTranslation,
                ),
            )
        }

        is TranslationsAction.SetEngineSupportedAction -> {
            state.copy(
                translationEngine = state.translationEngine.copy(
                    isEngineSupported = action.isEngineSupported,
                    engineError = null,
                ),
            )
        }

        is TranslationsAction.FetchTranslationDownloadSizeAction -> {
            state.copyWithTranslationsState(action.tabId) {
                it.copy(
                    translationDownloadSize = null,
                )
            }
        }

        is TranslationsAction.SetTranslationDownloadSizeAction -> {
            state.copyWithTranslationsState(action.tabId) {
                it.copy(
                    translationDownloadSize = action.translationSize,
                )
            }
        }

        is TranslationsAction.SetLanguageSettingsAction -> {
            state.copy(
                translationEngine = state.translationEngine.copy(
                    languageSettings = action.languageSettings,
                    engineError = null,
                ),
            )
        }

        is TranslationsAction.ManageLanguageModelsAction -> {
            val processState = if (action.options.operation == ModelOperation.DOWNLOAD) {
                ModelState.DOWNLOAD_IN_PROGRESS
            } else {
                ModelState.DELETION_IN_PROGRESS
            }
            val newModelState = LanguageModel.determineNewLanguageModelState(
                currentLanguageModels = state.translationEngine.languageModels,
                options = action.options,
                newStatus = processState,
            )
            state.copy(
                translationEngine = state.translationEngine.copy(
                    languageModels = newModelState,
                ),
            )
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
