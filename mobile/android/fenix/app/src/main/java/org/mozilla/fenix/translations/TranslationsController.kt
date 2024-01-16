/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations

import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.translate.DetectedLanguages
import mozilla.components.concept.engine.translate.TranslationOptions
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.support.base.log.logger.Logger

/**
 * Manages requests to complete operations with other components.
 *
 * @param translationUseCase The use case for performing a translation.
 * @param browserStore The browser store to use for this controller.
 * @param tabId The tab to perform operations or complete requests for.
 */
class TranslationsController(
    private val translationUseCase: SessionUseCases.TranslateUseCase,
    private val browserStore: BrowserStore,
    private val tabId: String,

) {
    private val logger = Logger("TranslationsController")

    /**
     * Retrieves detected information about the language on the browser page, the user's preferred
     * language, and if the detected page language is supported.
     *
     * @return The [DetectedLanguages] object that contains page and user preference information.
     */
    fun getDetectedLanguages(): DetectedLanguages? {
        logger.info("Retrieving translations language support from the browser store.")
        return browserStore.state.findTab(tabId)?.translationsState?.translationEngineState?.detectedLanguages
    }

    /**
     * Translates the page on the given [tabId]. Will fallback to default expectations if
     * [fromLanguage] and [toLanguage] are not provided.
     *
     * @param tabId The ID of the tab to translate.
     * @param fromLanguage The BCP-47 language code to translate from. If null, the default will
     * be set to the page language.
     * @param toLanguage The BCP-47 language code to translate to. If null, the default will
     * be set to the user's preferred language.
     * @param options Optional options to specify and additional criteria for the translation.
     */
    fun translate(
        tabId: String?,
        fromLanguage: String?,
        toLanguage: String?,
        options: TranslationOptions?,
    ) {
        if (fromLanguage != null && toLanguage != null) {
            logger.info("Requesting a translation.")
            translationUseCase.invoke(tabId, fromLanguage, toLanguage, options)
            return
        }

        // Fallback to find defaults
        var defaultFromLanguage = fromLanguage
        var defaultToLanguage = toLanguage
        val detectedLanguages = getDetectedLanguages()

        if (defaultFromLanguage == null) {
            defaultFromLanguage = detectedLanguages?.documentLangTag
            logger.info("Setting translating to use a default 'from' of $defaultFromLanguage.")
        }

        if (defaultToLanguage == null) {
            defaultToLanguage = detectedLanguages?.userPreferredLangTag
            logger.info("Setting translating to use a default 'to' of $defaultToLanguage.")
        }

        if (defaultFromLanguage != null && defaultToLanguage != null) {
            logger.info("Requesting a translation based on defaults.")
            translationUseCase.invoke(tabId, defaultFromLanguage, defaultToLanguage, options)
            return
        }
    }
}
