/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations

import org.mozilla.fenix.R

/**
 * TranslationSwitchItem that will appear on Translation screens.
 * @property type [TranslationSettingsOption] type depending on the screen.
 * In [TranslationOptionsDialog] is TranslationPageSettingsOption and in [TranslationSettings] is
 * [TranslationSettingsScreenOption].
 * @property textLabel The text that will appear on the switch item.
 * @property isChecked Whether the switch is checked or not.
 * @property isEnabled Whether the switch is enabled or not.
 * @property onStateChange Invoked when the switch item is clicked,
 * the new checked state is passed into the callback.
 */
data class TranslationSwitchItem(
    val type: TranslationSettingsOption,
    val textLabel: String,
    var isChecked: Boolean,
    var isEnabled: Boolean = true,
    val onStateChange: (TranslationSettingsOption, Boolean) -> Unit,
)

/**
 * Translation settings that is related to the web-page. It will appear in [TranslationsOptionsDialog].
 */
sealed class TranslationPageSettingsOption(
    override val descriptionId: Int? = null,
    override val hasDivider: Boolean,
) : TranslationSettingsOption(hasDivider = hasDivider) {

    /**
     * The system should offer a translation on a page.
     */
    data class AlwaysOfferPopup(
        override val hasDivider: Boolean = true,
    ) : TranslationPageSettingsOption(hasDivider = hasDivider)

    /**
     * The page's always translate language setting.
     */
    data class AlwaysTranslateLanguage(
        override val hasDivider: Boolean = false,
        override val descriptionId: Int = R.string.translation_option_bottom_sheet_switch_description,
    ) : TranslationPageSettingsOption(hasDivider = hasDivider)

    /**
     * The page's never translate language setting.
     */
    data class NeverTranslateLanguage(
        override val hasDivider: Boolean = true,
        override val descriptionId: Int = R.string.translation_option_bottom_sheet_switch_description,
    ) : TranslationPageSettingsOption(hasDivider = hasDivider)

    /**
     *  The page's never translate site setting.
     */
    data class NeverTranslateSite(
        override val hasDivider: Boolean = true,
        override val descriptionId:
        Int = R.string.translation_option_bottom_sheet_switch_never_translate_site_description,
    ) : TranslationPageSettingsOption(hasDivider = hasDivider)
}

/**
 * @property hasDivider Organizes translation settings toggle layouts.
 * Whether a divider should appear under the switch item.
 * @property descriptionId An optional description text id below the label.
 *
 */
sealed class TranslationSettingsOption(
    open val hasDivider: Boolean,
    open val descriptionId: Int? = null,
)

/**
 * Translation settings that is related to the web-page.It will appear in [TranslationSettings].
 */
sealed class TranslationSettingsScreenOption {
    /**
     *  The page's offer to translate when possible.
     */
    data class OfferToTranslate(override val hasDivider: Boolean = true) :
        TranslationSettingsOption(hasDivider = hasDivider)

    /**
     *  The page's always download languages in data saving mode.
     */
    data class AlwaysDownloadInSavingMode(override val hasDivider: Boolean = true) :
        TranslationSettingsOption(hasDivider = hasDivider)
}
