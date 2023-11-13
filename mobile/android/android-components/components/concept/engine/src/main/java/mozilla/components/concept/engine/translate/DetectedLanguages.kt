/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.translate

/**
* The representation of a translations detected document and user language.
*
* @property documentLangTag The auto-detected language tag of page. Usually used for determining the
* best guess for translating "from".
* @property supportedDocumentLang If the translation engine supports the document language.
* @property userPreferredLangTag The user's preferred language tag. Usually used for determining the
 * best guess for translating "to".
*/
data class DetectedLanguages(
    val documentLangTag: String? = null,
    val supportedDocumentLang: Boolean? = false,
    val userPreferredLangTag: String? = null,
)
