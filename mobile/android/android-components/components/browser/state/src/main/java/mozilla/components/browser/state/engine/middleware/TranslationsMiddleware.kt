/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.engine.middleware

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.TranslationsAction
import mozilla.components.browser.state.action.TranslationsAction.TranslateExpectedAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.translate.TranslationError
import mozilla.components.concept.engine.translate.TranslationOperation
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.support.base.log.logger.Logger

/**
 * This middleware is for use with managing any states or resources required for translating a
 * webpage.
 */
class TranslationsMiddleware(
    private val engine: Engine,
    private val scope: CoroutineScope,
) : Middleware<BrowserState, BrowserAction> {
    private val logger = Logger("TranslationsMiddleware")

    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction,
    ) {
        // Pre process actions
        when (action) {
            is TranslateExpectedAction -> {
                scope.launch {
                    requestSupportedLanguages(context, action.tabId)
                }
            }
            else -> {
                // no-op
            }
        }

        // Continue to post process actions
        next(action)
    }

    /**
     * Retrieves the list of supported languages using [scope] and dispatches the result to the
     * store via [TranslationsAction.TranslateSetLanguagesAction] or else dispatches the failure
     * [TranslationsAction.TranslateExceptionAction].
     *
     * @param context Context to use to dispatch to the store.
     * @param tabId Tab ID associated with the request.
     */
    private suspend fun requestSupportedLanguages(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        tabId: String,
    ) = withContext(Dispatchers.IO) {
        scope.launch {
            engine.getSupportedTranslationLanguages(

                onSuccess = {
                    context.store.dispatch(
                        TranslationsAction.TranslateSetLanguagesAction(
                            tabId = tabId,
                            supportedLanguages = it,
                        ),
                    )
                    logger.info("Success requesting supported languages.")
                },

                onError = {
                    context.store.dispatch(
                        TranslationsAction.TranslateExceptionAction(
                            tabId = tabId,
                            operation = TranslationOperation.FETCH_LANGUAGES,
                            translationError = TranslationError.CouldNotLoadLanguagesError(it),
                        ),
                    )
                    logger.error("Error requesting supported languages: ", it)
                },
            )
        }
    }
}
