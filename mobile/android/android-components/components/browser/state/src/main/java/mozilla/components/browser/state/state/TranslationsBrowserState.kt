/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

import mozilla.components.concept.engine.translate.LanguageModel
import mozilla.components.concept.engine.translate.LanguageSetting
import mozilla.components.concept.engine.translate.TranslationError
import mozilla.components.concept.engine.translate.TranslationSupport

/**
 * Value type that represents the state of the translations engine within a [BrowserState].
 *
 * @property isEngineSupported Whether the translations engine supports the device architecture.
 * @property offerTranslation Whether to offer translations or not to the user.
 * @property supportedLanguages Set of languages the translation engine supports.
 * @property languageModels Set of language machine learning translation models the translation engine has available.
 * @property languageSettings A map containing a key of BCP 47 language code and its
 * [LanguageSetting] to represent the automatic language settings.
 * @property neverTranslateSites List of sites the user has opted to never translate.
 * @property engineError Holds the error state of the translations engine.
 * See [TranslationsState.translationError] for session level errors.
 */
data class TranslationsBrowserState(
    val isEngineSupported: Boolean? = null,
    val offerTranslation: Boolean? = null,
    val supportedLanguages: TranslationSupport? = null,
    val languageModels: List<LanguageModel>? = null,
    val languageSettings: Map<String, LanguageSetting>? = null,
    val neverTranslateSites: List<String>? = null,
    val engineError: TranslationError? = null,
)
