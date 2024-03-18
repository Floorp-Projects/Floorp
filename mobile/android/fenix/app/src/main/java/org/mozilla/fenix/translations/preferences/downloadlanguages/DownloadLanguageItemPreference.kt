/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations.preferences.downloadlanguages

import android.os.Parcelable
import kotlinx.parcelize.Parcelize
import org.mozilla.geckoview.TranslationsController

/**
 * DownloadLanguageItemPreference that will appear on [DownloadLanguagesPreferenceFragment] screen.
 *
 * @property languageModel The object comes from the translation engine.
 * @property state State of the [languageModel].
 */
data class DownloadLanguageItemPreference(
    val languageModel: TranslationsController.RuntimeTranslation.LanguageModel,
    val state: DownloadLanguageItemStatePreference,
)

/**
 * DownloadLanguageItemStatePreference the state of the [DownloadLanguageItemPreference]
 *
 * @property type The type of the language item.
 * @property status State of the language file.
 */
@Parcelize
data class DownloadLanguageItemStatePreference(
    val type: DownloadLanguageItemTypePreference,
    val status: DownloadLanguageItemStatusPreference,
) : Parcelable

/**
 * DownloadLanguageItemTypePreference the type of the [DownloadLanguageItemPreference]
 */
enum class DownloadLanguageItemTypePreference {
    PivotLanguage,
    AllLanguages,
    GeneralLanguage,
}

/**
 * DownloadLanguageItemStatusPreference the status of the [DownloadLanguageItemPreference]
 */
enum class DownloadLanguageItemStatusPreference {
    Downloaded,
    NotDownloaded,
    DownloadInProgress,
    Selected,
}
