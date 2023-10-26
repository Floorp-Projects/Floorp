/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.translate

/**
 * Translation options that map to the Gecko Translations Options.
 *
 * @property downloadModel If the necessary models should be downloaded on request. If false, then
 * the translation will not complete and throw an exception if the models are not already available.
 */
data class TranslationOptions(
    val downloadModel: Boolean = true,
)
