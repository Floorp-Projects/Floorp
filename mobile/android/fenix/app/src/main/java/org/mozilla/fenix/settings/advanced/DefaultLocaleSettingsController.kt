/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.advanced

import android.app.Activity
import android.content.Context
import mozilla.components.support.locale.LocaleManager
import java.util.Locale

interface LocaleSettingsController {
    fun handleLocaleSelected(locale: Locale)
    fun handleSearchQueryTyped(query: String)
    fun handleDefaultLocaleSelected()
}

class DefaultLocaleSettingsController(
    private val context: Context,
    private val localeSettingsStore: LocaleSettingsStore
) : LocaleSettingsController {

    override fun handleLocaleSelected(locale: Locale) {
        if (localeSettingsStore.state.selectedLocale == locale &&
            !LocaleManager.isDefaultLocaleSelected(context)) {
            return
        }
        localeSettingsStore.dispatch(LocaleSettingsAction.Select(locale))
        LocaleManager.setNewLocale(context, locale.toLanguageTag())
        LocaleManager.updateBaseConfiguration(context, locale)
        (context as Activity).recreate()
    }

    override fun handleDefaultLocaleSelected() {
        if (LocaleManager.isDefaultLocaleSelected(context)) {
            return
        }
        localeSettingsStore.dispatch(LocaleSettingsAction.Select(localeSettingsStore.state.localeList[0]))
        LocaleManager.resetToSystemDefault(context)
        LocaleManager.updateBaseConfiguration(context, localeSettingsStore.state.localeList[0])
        (context as Activity).recreate()
    }

    override fun handleSearchQueryTyped(query: String) {
        localeSettingsStore.dispatch(LocaleSettingsAction.Search(query))
    }
}
