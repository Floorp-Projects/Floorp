/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations.preferences.downloadlanguages

import mozilla.components.concept.engine.translate.LanguageModel

/**
 * DownloadLanguageItemPreference that will appear on [DownloadLanguagesPreferenceFragment] screen.
 *
 * @property languageModel The object comes from the translation engine.
 * @property type The type of the language item.
 * @property enabled Whether the item is enabled or not.
 *
 */
data class DownloadLanguageItemPreference(
    val languageModel: LanguageModel,
    val type: DownloadLanguageItemTypePreference,
    val enabled: Boolean = true,
)

/**
 * DownloadLanguageItemTypePreference the type of the [DownloadLanguageItemPreference]
 */
enum class DownloadLanguageItemTypePreference {
    PivotLanguage,
    AllLanguages,
    GeneralLanguage,
}
