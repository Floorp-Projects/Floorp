/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.translate

/**
 * A data class to contain information related to the download size required for a given
 * translation to/from pair.
 *
 * For the translations engine to complete a translation on a specified to/from pair,
 * first, the necessary ML models must be downloaded to the device.
 * This class represents the download state of the ML models necessary to translate the
 * given to/from pair.
 *
 * @property fromLanguage The [Language] to translate from on a given translation.
 * @property toLanguage The [Language] to translate to on a given translation.
 * @property size The size of the download to perform the translation in bytes. Null means the value has
 * yet to be received or an error occurred. Zero means no download required or else a model does not exist.
 * @property error The [TranslationError] reported if an error occurred while fetching the size.
 */
data class TranslationDownloadSize(
    val fromLanguage: Language,
    val toLanguage: Language,
    val size: Long? = null,
    val error: TranslationError? = null,
)
