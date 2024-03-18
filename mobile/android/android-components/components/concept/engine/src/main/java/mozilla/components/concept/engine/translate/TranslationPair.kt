/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.translate

/**
* The representation of the translation state.
*
* @property fromLanguage The language the page is translated from originally.
* @property toLanguage The language the page is translated to that the user knows.
*/
data class TranslationPair(
    val fromLanguage: String? = null,
    val toLanguage: String? = null,
)
