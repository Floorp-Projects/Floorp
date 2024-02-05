/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.engine.middleware

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.TranslationsAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.translate.LanguageSetting
import mozilla.components.concept.engine.translate.TranslationError
import mozilla.components.concept.engine.translate.TranslationOperation
import mozilla.components.concept.engine.translate.TranslationPageSettings
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.support.base.log.logger.Logger
import kotlin.coroutines.resume
import kotlin.coroutines.suspendCoroutine

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
            is TranslationsAction.OperationRequestedAction -> {
                when (action.operation) {
                    TranslationOperation.FETCH_SUPPORTED_LANGUAGES -> {
                        scope.launch {
                            requestSupportedLanguages(context, action.tabId)
                        }
                    }
                    TranslationOperation.FETCH_PAGE_SETTINGS -> {
                        scope.launch {
                            requestTranslationPageSettings(context, action.tabId)
                        }
                    }
                    TranslationOperation.FETCH_NEVER_TRANSLATE_SITES -> {
                        scope.launch {
                            getNeverTranslateSites(context, action.tabId)
                        }
                    }
                    TranslationOperation.TRANSLATE,
                    TranslationOperation.RESTORE,
                    -> Unit
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
    private fun requestSupportedLanguages(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        tabId: String,
    ) {
        engine.getSupportedTranslationLanguages(

            onSuccess = {
                context.store.dispatch(
                    TranslationsAction.SetSupportedLanguagesAction(
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
                        operation = TranslationOperation.FETCH_SUPPORTED_LANGUAGES,
                        translationError = TranslationError.CouldNotLoadLanguagesError(it),
                    ),
                )
                logger.error("Error requesting supported languages: ", it)
            },
        )
    }

    /**
     * Retrieves the list of never translate sites using [scope] and dispatches the result to the
     * store via [TranslationsAction.SetNeverTranslateSitesAction] or else dispatches the failure
     * [TranslationsAction.TranslateExceptionAction].
     *
     * @param context Context to use to dispatch to the store.
     * @param tabId Tab ID associated with the request.
     */
    private fun getNeverTranslateSites(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        tabId: String,
    ) {
        engine.getNeverTranslateSiteList(
            onSuccess = {
                context.store.dispatch(
                    TranslationsAction.SetNeverTranslateSitesAction(
                        tabId = tabId,
                        neverTranslateSites = it,
                    ),
                )
                logger.info("Success requesting never translate sites.")
            },

            onError = {
                context.store.dispatch(
                    TranslationsAction.TranslateExceptionAction(
                        tabId = tabId,
                        operation = TranslationOperation.FETCH_NEVER_TRANSLATE_SITES,
                        translationError = TranslationError.CouldNotLoadNeverTranslateSites(it),
                    ),
                )
                logger.error("Error requesting never translate sites: ", it)
            },
        )
    }

    /**
     * Retrieves the page settings using [scope] and dispatches the result to the
     * store via [TranslationsAction.SetPageSettingsAction] or else dispatches the failure
     * [TranslationsAction.TranslateExceptionAction].
     *
     * @param context Context to use to dispatch to the store.
     * @param tabId Tab ID associated with the request.
     */
    private suspend fun requestTranslationPageSettings(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        tabId: String,
    ) {
        // Always offer setting
        val alwaysOfferPopup: Boolean = engine.getTranslationsOfferPopup()

        // Page language settings
        val pageLanguage = context.store.state.findTab(tabId)
            ?.translationsState?.translationEngineState?.detectedLanguages?.documentLangTag
        val setting = pageLanguage?.let { getLanguageSetting(it) }
        val alwaysTranslateLanguage = setting?.toBoolean(LanguageSetting.ALWAYS)
        val neverTranslateLanguage = setting?.toBoolean(LanguageSetting.NEVER)

        // Never translate site
        val engineSession = context.store.state.findTab(tabId)
            ?.engineState?.engineSession
        val neverTranslateSite = engineSession?.let { getNeverTranslateSiteSetting(it) }

        if (
            alwaysTranslateLanguage != null &&
            neverTranslateLanguage != null &&
            neverTranslateSite != null
        ) {
            TranslationsAction.SetPageSettingsAction(
                tabId = tabId,
                pageSettings =
                TranslationPageSettings(
                    alwaysOfferPopup = alwaysOfferPopup,
                    alwaysTranslateLanguage = alwaysTranslateLanguage,
                    neverTranslateLanguage = neverTranslateLanguage,
                    neverTranslateSite = neverTranslateSite,
                ),
            )
        } else {
            // Any null values indicate something went wrong, alert an error occurred
            TranslationsAction.TranslateExceptionAction(
                tabId = tabId,
                operation = TranslationOperation.FETCH_PAGE_SETTINGS,
                translationError = TranslationError.CouldNotLoadPageSettingsError(null),
            )
        }
    }

    /**
     * Fetches the always or never language setting synchronously from the engine. Will
     * return null if an error occurs.
     *
     * @param pageLanguage Page language to check the translation preferences for.
     * @return The page translate language setting or null.
     */
    private suspend fun getLanguageSetting(pageLanguage: String): LanguageSetting? {
        return suspendCoroutine { continuation ->
            engine.getLanguageSetting(
                languageCode = pageLanguage,

                onSuccess = { setting ->
                    continuation.resume(setting)
                },

                onError = {
                    continuation.resume(null)
                },
            )
        }
    }

    /**
     * Fetches the never translate site setting synchronously from the [EngineSession]. Will
     * return null if an error occurs.
     *
     * @param engineSession With page context on how to complete this operation.
     * @return The never translate site setting from the [EngineSession] or null.
     */
    private suspend fun getNeverTranslateSiteSetting(engineSession: EngineSession): Boolean? {
        return suspendCoroutine { continuation ->
            engineSession.getNeverTranslateSiteSetting(
                onResult = { setting ->
                    continuation.resume(setting)
                },
                onException = {
                    continuation.resume(null)
                },
            )
        }
    }
}
