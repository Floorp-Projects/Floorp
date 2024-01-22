/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations

import mozilla.components.browser.state.action.TranslationsAction
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.translate.TranslationOperation
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext

/**
 * [Middleware] implementation for updating [BrowserStore] based on translation actions.
 */
class TranslationsDialogMiddleware(
    private val browserStore: BrowserStore,
    private val sessionId: String,
) : Middleware<TranslationsDialogState, TranslationsDialogAction> {

    override fun invoke(
        context: MiddlewareContext<TranslationsDialogState, TranslationsDialogAction>,
        next: (TranslationsDialogAction) -> Unit,
        action: TranslationsDialogAction,
    ) {
        when (action) {
            is TranslationsDialogAction.FetchSupportedLanguages -> {
                browserStore.dispatch(
                    TranslationsAction.OperationRequestedAction(
                        tabId = sessionId,
                        operation = TranslationOperation.FETCH_SUPPORTED_LANGUAGES,
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

            else -> {
                next(action)
            }
        }
    }
}
