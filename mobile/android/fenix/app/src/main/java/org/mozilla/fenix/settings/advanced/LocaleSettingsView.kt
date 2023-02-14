/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.advanced

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.recyclerview.widget.LinearLayoutManager
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.ComponentLocaleSettingsBinding
import java.util.Locale

interface LocaleSettingsViewInteractor {

    fun onLocaleSelected(locale: Locale)

    fun onDefaultLocaleSelected()

    fun onSearchQueryTyped(query: String)
}

class LocaleSettingsView(
    container: ViewGroup,
    val interactor: LocaleSettingsViewInteractor,
) {

    val view: View = LayoutInflater.from(container.context)
        .inflate(R.layout.component_locale_settings, container, true)

    val binding = ComponentLocaleSettingsBinding.bind(view)

    private val localeAdapter: LocaleAdapter

    init {
        binding.localeList.apply {
            localeAdapter = LocaleAdapter(interactor)
            adapter = localeAdapter
            layoutManager = LinearLayoutManager(context)
        }
    }

    fun update(state: LocaleSettingsState) {
        localeAdapter.updateData(state.searchedLocaleList, state.selectedLocale)
    }

    fun onResume() {
        view.requestFocus()
    }
}
