/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations

import mozilla.components.browser.state.action.TranslationsAction
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.translate.TranslationOperation
import mozilla.components.concept.engine.translate.TranslationPageSettingOperation
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext

/**
 * [Middleware] implementation for updating [BrowserStore] based on translation actions.
 */
class TranslationsDialogMiddleware(
    private val browserStore: BrowserStore,
    private val sessionId: String,
) : Middleware<TranslationsDialogState, TranslationsDialogAction> {

    @Suppress("LongMethod")
    override fun invoke(
        context: MiddlewareContext<TranslationsDialogState, TranslationsDialogAction>,
        next: (TranslationsDialogAction) -> Unit,
        action: TranslationsDialogAction,
    ) {
        when (action) {
            is TranslationsDialogAction.FetchDownloadFileSizeAction -> {
                browserStore.dispatch(
                    TranslationsAction.FetchTranslationDownloadSizeAction(
                        tabId = sessionId,
                        fromLanguage = action.fromLanguage,
                        toLanguage = action.toLanguage,
                    ),
                )
            }

            is TranslationsDialogAction.FetchSupportedLanguages -> {
                browserStore.dispatch(
                    TranslationsAction.OperationRequestedAction(
                        tabId = sessionId,
                        operation = TranslationOperation.FETCH_SUPPORTED_LANGUAGES,
                    ),
                )
            }

            is TranslationsDialogAction.FetchPageSettings -> {
                browserStore.dispatch(
                    TranslationsAction.OperationRequestedAction(
                        tabId = sessionId,
                        operation = TranslationOperation.FETCH_PAGE_SETTINGS,
                    ),
                )
            }

            is TranslationsDialogAction.TranslateAction -> {
                context.state.initialFrom?.code?.let { fromLanguage ->
                    context.state.initialTo?.code?.let { toLanguage ->
                        TranslationsAction.TranslateAction(
                            tabId = sessionId,
                            fromLanguage = fromLanguage,
                            toLanguage = toLanguage,
                            options = null,
                        )
                    }
                }?.let {
                    browserStore.dispatch(
                        it,
                    )
                }
            }

            is TranslationsDialogAction.RestoreTranslation -> {
                browserStore.dispatch(TranslationsAction.TranslateRestoreAction(sessionId))
            }

            is TranslationsDialogAction.UpdatePageSettingsValue -> {
                when (action.type) {
                    is TranslationPageSettingsOption.AlwaysOfferPopup -> browserStore.dispatch(
                        TranslationsAction.UpdatePageSettingAction(
                            tabId = sessionId,
                            operation = TranslationPageSettingOperation.UPDATE_ALWAYS_OFFER_POPUP,
                            setting = action.checkValue,
                        ),
                    )

                    is TranslationPageSettingsOption.AlwaysTranslateLanguage -> browserStore.dispatch(
                        TranslationsAction.UpdatePageSettingAction(
                            tabId = sessionId,
                            operation = TranslationPageSettingOperation.UPDATE_ALWAYS_TRANSLATE_LANGUAGE,
                            setting = action.checkValue,
                        ),
                    )

                    is TranslationPageSettingsOption.NeverTranslateLanguage -> browserStore.dispatch(
                        TranslationsAction.UpdatePageSettingAction(
                            tabId = sessionId,
                            operation = TranslationPageSettingOperation.UPDATE_NEVER_TRANSLATE_LANGUAGE,
                            setting = action.checkValue,
                        ),
                    )

                    is TranslationPageSettingsOption.NeverTranslateSite -> browserStore.dispatch(
                        TranslationsAction.UpdatePageSettingAction(
                            tabId = sessionId,
                            operation = TranslationPageSettingOperation.UPDATE_NEVER_TRANSLATE_SITE,
                            setting = action.checkValue,
                        ),
                    )
                }
            }

            else -> {
                next(action)
            }
        }
    }
}
