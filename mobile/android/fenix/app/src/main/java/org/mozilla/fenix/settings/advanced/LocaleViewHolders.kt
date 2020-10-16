/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.advanced

import android.view.View
import androidx.annotation.VisibleForTesting
import androidx.core.view.isVisible
import kotlinx.android.synthetic.main.locale_settings_item.*
import mozilla.components.support.locale.LocaleManager
import org.mozilla.fenix.R
import org.mozilla.fenix.utils.view.ViewHolder
import java.util.Locale

class LocaleViewHolder(
    view: View,
    selectedLocale: Locale,
    private val interactor: LocaleSettingsViewInteractor
) : BaseLocaleViewHolder(view, selectedLocale) {

    override fun bind(locale: Locale) {
        // capitalisation is done using the rules of the appropriate locale (endonym and exonym)
        locale_title_text.text = locale.getDisplayName(locale).capitalize(locale)
        locale_subtitle_text.text = locale.displayName.capitalize(Locale.getDefault())
        locale_selected_icon.isVisible = isCurrentLocaleSelected(locale, isDefault = false)

        itemView.setOnClickListener {
            interactor.onLocaleSelected(locale)
        }
    }
}

class SystemLocaleViewHolder(
    view: View,
    selectedLocale: Locale,
    private val interactor: LocaleSettingsViewInteractor
) : BaseLocaleViewHolder(view, selectedLocale) {

    override fun bind(locale: Locale) {
        locale_title_text.text = itemView.context.getString(R.string.default_locale_text)
        locale_subtitle_text.text = locale.displayName.capitalize(Locale.getDefault())
        locale_selected_icon.isVisible = isCurrentLocaleSelected(locale, isDefault = true)
        itemView.setOnClickListener {
            interactor.onDefaultLocaleSelected()
        }
    }
}

abstract class BaseLocaleViewHolder(
    view: View,
    private val selectedLocale: Locale
) : ViewHolder(view) {

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    internal fun isCurrentLocaleSelected(locale: Locale, isDefault: Boolean): Boolean {
        return if (isDefault) {
            locale == selectedLocale && LocaleManager.isDefaultLocaleSelected(itemView.context)
        } else {
            locale == selectedLocale && !LocaleManager.isDefaultLocaleSelected(itemView.context)
        }
    }

    abstract fun bind(locale: Locale)
}

/**
 * Similar to Kotlin's capitalize with locale parameter, but that method is currently experimental
 */
private fun String.capitalize(locale: Locale): String {
    return substring(0, 1).toUpperCase(locale) + substring(1)
}
