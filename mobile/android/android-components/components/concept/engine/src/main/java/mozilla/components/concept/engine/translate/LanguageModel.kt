/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.translate

/**
 * The language model container for representing language model state to the user.
 *
 * Please note, a single LanguageModel is usually comprised of
 * an aggregation of multiple machine learning models on the translations engine level. The engine
 * has already handled this abstraction.
 *
 * @property language The specified language the language model set can process.
 * @property isDownloaded If all the necessary models are downloaded.
 * @property size The size of the total model download(s).
 */
data class LanguageModel(
    val language: Language? = null,
    val isDownloaded: Boolean = false,
    val size: Long? = null,
)
