/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations

import mozilla.components.concept.engine.translate.DetectedLanguages
import mozilla.components.concept.engine.translate.TranslationOptions
import mozilla.components.support.base.log.logger.Logger

/**
 * Manages coordinating the functionality interactions for use in the views.
 *
 * @param translationsController The translations controller that requests data and
 * interactions.
 */
class TranslationsInteractor(
    private val translationsController: TranslationsController,
) {
    private val logger = Logger("TranslationsInteractor")

    /**
     * Retrieves the [DetectedLanguages] applicable to the page.
     *
     * @return The [DetectedLanguages] object that contains page and user preference information.
     */
    fun detectedLanguages(): DetectedLanguages? {
        logger.info("Requesting translations language support from the controller.")
        return translationsController.getDetectedLanguages()
    }

    /**
     * Translates the page on the given [tabId].
     * If null is provided for [fromLanguage] or [toLanguage], the engine will attempt to
     * find a sensible default.
     *
     * @param tabId The ID of the tab to translate.
     * @param fromLanguage The BCP-47 language code to translate from. Usually will be the detected
     * page language. If set as null, will revert to a default page language, if known.
     * @param toLanguage The BCP-47 language code to translate to. Usually will be the user's
     * preferred language. If set as null, will revert to a default of the user's preferred
     * language, if known.
     * @param options Optional options to specify and additional criteria for the translation.
     */
    fun onTranslate(
        tabId: String?,
        fromLanguage: String?,
        toLanguage: String?,
        options: TranslationOptions?,
    ) {
        logger.info("Requesting a translation from the controller.")
        translationsController.translate(tabId, fromLanguage, toLanguage, options)
    }
}
