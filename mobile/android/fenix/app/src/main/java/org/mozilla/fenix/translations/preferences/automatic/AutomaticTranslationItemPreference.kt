/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations.preferences.automatic

import android.os.Parcelable
import kotlinx.parcelize.Parcelize
import org.mozilla.fenix.R

/**
 * AutomaticTranslationItem that will appear on Automatic Translation screen.
 *
 * @property displayName The text that will appear in the list.
 * @property automaticTranslationOptionPreference The option that the user selected.
 */
@Parcelize
data class AutomaticTranslationItemPreference(
    val displayName: String,
    val automaticTranslationOptionPreference: AutomaticTranslationOptionPreference,
) : Parcelable

/**
 * AutomaticTranslationOption for a language.
 *
 * @property titleId The string id title of the option.
 * @property summaryId The string id summary of the option.
 */
@Parcelize
sealed class AutomaticTranslationOptionPreference(
    open val titleId: Int,
    open val summaryId: List<Int>,
) : Parcelable {

    /**
     * The app will offer to translate sites in the selected language.
     */
    data class OfferToTranslate(
        override val titleId: Int = R.string.automatic_translation_option_offer_to_translate_title_preference,
        override val summaryId: List<Int> = listOf(
            R.string.automatic_translation_option_offer_to_translate_summary_preference,
            R.string.firefox,
        ),
    ) : AutomaticTranslationOptionPreference(titleId = titleId, summaryId = summaryId)

    /**
     * The app will translate in the selected language automatically when the page loads.
     */
    data class AlwaysTranslate(
        override val titleId: Int = R.string.automatic_translation_option_always_translate_title_preference,
        override val summaryId: List<Int> = listOf(
            R.string.automatic_translation_option_always_translate_summary_preference,
            R.string.firefox,
        ),
    ) : AutomaticTranslationOptionPreference(titleId = titleId, summaryId = summaryId)

    /**
     * The app will never offer to translate sites in the selected language.
     */
    data class NeverTranslate(
        override val titleId: Int = R.string.automatic_translation_option_never_translate_title_preference,
        override val summaryId: List<Int> = listOf(
            R.string.automatic_translation_option_never_translate_summary_preference,
            R.string.firefox,
        ),
    ) : AutomaticTranslationOptionPreference(titleId = titleId, summaryId = summaryId)
}
