/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.advanced

import java.util.Locale

class LocaleSettingsInteractor(private val controller: LocaleSettingsController) :
    LocaleSettingsViewInteractor {

    override fun onLocaleSelected(locale: Locale) {
        controller.handleLocaleSelected(locale)
    }

    override fun onDefaultLocaleSelected() {
        controller.handleDefaultLocaleSelected()
    }

    override fun onSearchQueryTyped(query: String) {
        controller.handleSearchQueryTyped(query)
    }
}
