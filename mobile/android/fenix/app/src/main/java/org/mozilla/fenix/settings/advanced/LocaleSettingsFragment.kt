/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.advanced

import android.os.Bundle
import android.view.LayoutInflater
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import android.view.inputmethod.EditorInfo
import androidx.appcompat.widget.SearchView
import androidx.core.view.MenuProvider
import androidx.fragment.app.Fragment
import androidx.lifecycle.Lifecycle
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.support.ktx.android.view.hideKeyboard
import mozilla.components.support.locale.LocaleUseCases
import org.mozilla.fenix.R
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.databinding.FragmentLocaleSettingsBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.showToolbar

class LocaleSettingsFragment : Fragment(), MenuProvider {

    private lateinit var localeSettingsStore: LocaleSettingsStore
    private lateinit var interactor: LocaleSettingsInteractor
    private lateinit var localeView: LocaleSettingsView

    private var _binding: FragmentLocaleSettingsBinding? = null
    private val binding get() = _binding!!

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        _binding = FragmentLocaleSettingsBinding.inflate(inflater, container, false)
        val view = binding.root

        val browserStore = requireContext().components.core.store
        val localeUseCase = LocaleUseCases(browserStore)

        localeSettingsStore = StoreProvider.get(this) {
            LocaleSettingsStore(
                createInitialLocaleSettingsState(requireContext()),
            )
        }
        interactor = LocaleSettingsInteractor(
            controller = DefaultLocaleSettingsController(
                activity = requireActivity(),
                localeSettingsStore = localeSettingsStore,
                browserStore = browserStore,
                localeUseCase = localeUseCase,
            ),
        )
        localeView = LocaleSettingsView(binding.root, interactor)
        return view
    }

    override fun onCreateMenu(menu: Menu, inflater: MenuInflater) {
        inflater.inflate(R.menu.languages_list, menu)
        val searchItem = menu.findItem(R.id.search)
        val searchView: SearchView = searchItem.actionView as SearchView
        searchView.imeOptions = EditorInfo.IME_ACTION_DONE
        searchView.queryHint = getString(R.string.locale_search_hint)
        searchView.maxWidth = Int.MAX_VALUE

        searchView.setOnQueryTextListener(
            object : SearchView.OnQueryTextListener {
                override fun onQueryTextSubmit(query: String): Boolean {
                    return false
                }

                override fun onQueryTextChange(newText: String): Boolean {
                    interactor.onSearchQueryTyped(newText)
                    return false
                }
            },
        )
    }

    override fun onMenuItemSelected(menuItem: MenuItem): Boolean = false

    override fun onResume() {
        super.onResume()
        localeView.onResume()
        showToolbar(getString(R.string.preferences_language))
    }

    override fun onPause() {
        view?.hideKeyboard()
        super.onPause()
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        requireActivity().addMenuProvider(this, viewLifecycleOwner, Lifecycle.State.RESUMED)

        consumeFrom(localeSettingsStore) {
            localeView.update(it)
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()

        _binding = null
    }
}
