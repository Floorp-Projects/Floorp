/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.search

import android.content.Context
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import androidx.navigation.fragment.findNavController
import kotlinx.coroutines.MainScope
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.support.ktx.android.view.hideKeyboard
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.FragmentSearchShortcutsBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getRootView
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme
import org.mozilla.fenix.utils.allowUndo

/**
 * A [Fragment] that allows user to select what search engine shortcuts will be visible in the quick
 * search menu.
 */
class SearchShortcutsFragment : Fragment(R.layout.fragment_search_shortcuts) {

    private var _binding: FragmentSearchShortcutsBinding? = null
    private val binding get() = _binding!!

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        _binding = FragmentSearchShortcutsBinding.inflate(
            inflater,
            container,
            false,
        )

        binding.root.setContent {
            FirefoxTheme(theme = Theme.getTheme(allowPrivateTheme = false)) {
                SearchEngineShortcuts(
                    getString(R.string.preferences_category_engines_in_search_menu),
                    requireComponents.core.store,
                    onEditEngineClicked = {
                        navigateToSaveEngineFragment(it)
                    },
                    onCheckboxClicked = { engine, isEnabled ->
                        requireContext().components.useCases.searchUseCases
                            .updateDisabledSearchEngineIds(
                                engine.id,
                                isEnabled,
                            )
                    },
                    onDeleteEngineClicked = {
                        deleteSearchEngine(requireContext(), it)
                    },
                    onAddEngineClicked = {
                        navigateToSaveEngineFragment()
                    },
                )
            }
        }

        return binding.root
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    private fun navigateToSaveEngineFragment(engine: SearchEngine? = null) {
        val directions = SearchShortcutsFragmentDirections
            .actionSearchShortcutsFragmentToSaveSearchEngineFragment(engine?.id)

        findNavController().navigate(directions)
    }

    private fun deleteSearchEngine(
        context: Context,
        engine: SearchEngine,
    ) {
        context.components.useCases.searchUseCases.removeSearchEngine(engine)

        MainScope().allowUndo(
            view = context.getRootView()!!,
            message = context
                .getString(R.string.search_delete_search_engine_success_message, engine.name),
            undoActionTitle = context.getString(R.string.snackbar_deleted_undo),
            onCancel = {
                context.components.useCases.searchUseCases.addSearchEngine(engine)
            },
            operation = {},
        )
    }

    override fun onResume() {
        super.onResume()
        view?.hideKeyboard()
        showToolbar(getString(R.string.preferences_manage_search_shortcuts_2))
    }
}
