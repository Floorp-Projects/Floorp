/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

import mozilla.components.concept.engine.translate.LanguageModel
import mozilla.components.concept.engine.translate.TranslationError
import mozilla.components.concept.engine.translate.TranslationSupport

/**
 * Value type that represents the state of the translations engine within a [BrowserState].
 *
 * @property isEngineSupported Whether the translations engine supports the device architecture.
 * @property supportedLanguages Set of languages the translation engine supports.
 * @property languageModels Set of language machine learning translation models the translation engine has available.
 * @property engineError Holds the error state of the translations engine.
 * See [TranslationsState.translationError] for session level errors.
 */
data class TranslationsBrowserState(
    val isEngineSupported: Boolean? = null,
    val supportedLanguages: TranslationSupport? = null,
    val languageModels: List<LanguageModel>? = null,
    val engineError: TranslationError? = null,
)
