/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.advanced

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import kotlinx.android.synthetic.main.fragment_locale_settings.view.locale_container
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.support.ktx.android.view.hideKeyboard
import mozilla.components.support.locale.LocaleManager
import org.mozilla.fenix.R
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.ext.showToolbar

class LocaleSettingsFragment : Fragment() {

    private lateinit var store: LocaleSettingsStore
    private lateinit var interactor: LocaleSettingsInteractor
    private lateinit var localeView: LocaleSettingsView

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val view = inflater.inflate(R.layout.fragment_locale_settings, container, false)

        store = getStore()
        interactor = LocaleSettingsInteractor(
            controller = DefaultLocaleSettingsController(
                context = requireContext(),
                localeSettingsStore = store
            )
        )
        localeView = LocaleSettingsView(view.locale_container, interactor)
        return view
    }

    override fun onResume() {
        super.onResume()
        localeView.onResume()
        showToolbar(getString(R.string.preferences_language))
    }

    override fun onPause() {
        view?.hideKeyboard()
        super.onPause()
    }

    @ExperimentalCoroutinesApi
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        consumeFrom(store) {
            localeView.update(it)
        }
    }

    private fun getStore(): LocaleSettingsStore {
        val supportedLocales = LocaleManager.getSupportedLocales()
        val selectedLocale = LocaleManager.getSelectedLocale(requireContext())

        return StoreProvider.get(this) {
            LocaleSettingsStore(
                LocaleSettingsState(
                    supportedLocales,
                    supportedLocales,
                    selectedLocale
                )
            )
        }
    }
}
