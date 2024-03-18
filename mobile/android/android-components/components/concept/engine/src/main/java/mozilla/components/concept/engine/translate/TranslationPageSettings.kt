/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.translate

/**
 * Translation settings that relate to the page
 *
 * @property alwaysOfferPopup The setting for whether translations should automatically be offered.
 * When true, the engine will offer to translate the page if the detected translatable page language
 * is different from the user's preferred languages.
 * @property alwaysTranslateLanguage The setting for whether the current page language should be
 * automatically translated or not. When true, the page will automatically be translated by the
 * translations engine.
 * @property neverTranslateLanguage The setting for whether the current page language should offer a
 * translation or not. When true, the engine will not offer a translation.
 * @property neverTranslateSite The setting for whether the current site should be translated or not.
 * When true, the engine will not offer a translation on the current host site.
 */
data class TranslationPageSettings(
    val alwaysOfferPopup: Boolean? = null,
    val alwaysTranslateLanguage: Boolean? = null,
    val neverTranslateLanguage: Boolean? = null,
    val neverTranslateSite: Boolean? = null,
)
