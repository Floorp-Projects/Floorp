/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.translate

/**
* The representation of the translations engine state.
*
* @property detectedLanguages Detected information about preferences and page information.
* @property error If an error state occurred or an error was reported.
* @property isEngineReady If the translation engine is primed for use or will need to be loaded.
* @property requestedTranslationPair The language pair to translate. Usually populated after first request.
*/

data class TranslationEngineState(
    val detectedLanguages: DetectedLanguages? = null,
    val error: String? = null,
    val isEngineReady: Boolean? = false,
    val requestedTranslationPair: TranslationPair? = null,
)
