/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.translate

/**
 * The list of supported languages that may be translated to and translated from. Usually
 * a given language will be bi-directional (translate both to and from),
 * but this is not guaranteed, which is why the support response is two lists.
 *
 * @property fromLanguages The languages that the machine learning model may translate from.
 * @property toLanguages The languages that the machine learning model may translate to.
 */
data class TranslationSupport(
    val fromLanguages: List<Language>? = null,
    val toLanguages: List<Language>? = null,
)
