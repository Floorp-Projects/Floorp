/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.advanced

import android.os.Bundle
import android.view.LayoutInflater
import android.view.Menu
import android.view.MenuInflater
import android.view.View
import android.view.ViewGroup
import android.view.inputmethod.EditorInfo
import androidx.appcompat.widget.SearchView
import androidx.fragment.app.Fragment
import kotlinx.android.synthetic.main.fragment_locale_settings.view.*
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.support.ktx.android.view.hideKeyboard
import mozilla.components.support.locale.LocaleUseCases
import org.mozilla.fenix.R
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.showToolbar

class LocaleSettingsFragment : Fragment() {

    private lateinit var localeSettingsStore: LocaleSettingsStore
    private lateinit var interactor: LocaleSettingsInteractor
    private lateinit var localeView: LocaleSettingsView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setHasOptionsMenu(true)
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val view = inflater.inflate(R.layout.fragment_locale_settings, container, false)

        val browserStore = requireContext().components.core.store
        val localeUseCase = LocaleUseCases(browserStore)

        localeSettingsStore = StoreProvider.get(this) {
            LocaleSettingsStore(
                createInitialLocaleSettingsState(requireContext())
            )
        }
        interactor = LocaleSettingsInteractor(
            controller = DefaultLocaleSettingsController(
                activity = requireActivity(),
                localeSettingsStore = localeSettingsStore,
                localeUseCase = localeUseCase
            )
        )
        localeView = LocaleSettingsView(view.locale_container, interactor)
        return view
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        inflater.inflate(R.menu.languages_list, menu)
        val searchItem = menu.findItem(R.id.search)
        val searchView: SearchView = searchItem.actionView as SearchView
        searchView.imeOptions = EditorInfo.IME_ACTION_DONE
        searchView.queryHint = getString(R.string.locale_search_hint)

        searchView.setOnQueryTextListener(object : SearchView.OnQueryTextListener {
            override fun onQueryTextSubmit(query: String): Boolean {
                return false
            }

            override fun onQueryTextChange(newText: String): Boolean {
                interactor.onSearchQueryTyped(newText)
                return false
            }
        })
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
        consumeFrom(localeSettingsStore) {
            localeView.update(it)
        }
    }
}
