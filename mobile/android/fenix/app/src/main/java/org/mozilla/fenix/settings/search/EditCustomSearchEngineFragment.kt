/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.search

import android.net.Uri
import android.os.Bundle
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import kotlinx.android.synthetic.main.custom_search_engine.*
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.browser.search.SearchEngine
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.searchengine.CustomSearchEngineStore
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.settings.SupportUtils
import java.util.Locale

/**
 * Fragment to enter a custom search engine name and URL template.
 */
class EditCustomSearchEngineFragment : Fragment(R.layout.fragment_add_search_engine) {

    private val args by navArgs<EditCustomSearchEngineFragmentArgs>()

    private lateinit var searchEngine: SearchEngine

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setHasOptionsMenu(true)
        searchEngine = CustomSearchEngineStore.loadCustomSearchEngines(requireContext()).first {
            it.identifier == args.searchEngineIdentifier
        }
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        edit_engine_name.setText(searchEngine.name)
        val decodedUrl = Uri.decode(searchEngine.buildSearchUrl("%s"))
        edit_search_string.setText(decodedUrl)

        custom_search_engines_learn_more.setOnClickListener {
            (activity as HomeActivity).openToBrowserAndLoad(
                searchTermOrURL = SupportUtils.getSumoURLForTopic(
                    requireContext(),
                    SupportUtils.SumoTopic.CUSTOM_SEARCH_ENGINES
                ),
                newTab = true,
                from = BrowserDirection.FromEditCustomSearchEngineFragment
            )
        }
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.search_engine_edit_custom_search_engine_title))
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        inflater.inflate(R.menu.edit_custom_searchengine_menu, menu)
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            R.id.save_button -> {
                saveCustomEngine()
                true
            }
            else -> super.onOptionsItemSelected(item)
        }
    }

    private fun saveCustomEngine() {
        custom_search_engine_name_field.error = ""
        custom_search_engine_search_string_field.error = ""

        val name = edit_engine_name.text?.toString()?.trim() ?: ""
        val searchString = edit_search_string.text?.toString() ?: ""

        var hasError = false
        if (name.isEmpty()) {
            custom_search_engine_name_field.error = resources
                .getString(R.string.search_add_custom_engine_error_empty_name)
            hasError = true
        }

        val existingIdentifiers = requireComponents
            .search
            .provider
            .allSearchEngineIdentifiers()
            .map { it.toLowerCase(Locale.ROOT) }

        val nameHasChanged = name != args.searchEngineIdentifier

        if (existingIdentifiers.contains(name.toLowerCase(Locale.ROOT)) && nameHasChanged) {
            custom_search_engine_name_field.error = resources
                .getString(R.string.search_add_custom_engine_error_existing_name, name)
            hasError = true
        }

        if (searchString.isEmpty()) {
            custom_search_engine_search_string_field
                .error = resources.getString(R.string.search_add_custom_engine_error_empty_search_string)
            hasError = true
        }

        if (!searchString.contains("%s")) {
            custom_search_engine_search_string_field
                .error = resources.getString(R.string.search_add_custom_engine_error_missing_template)
            hasError = true
        }

        if (hasError) { return }

        viewLifecycleOwner.lifecycleScope.launch(Main) {
            val result = withContext(IO) {
                SearchStringValidator.isSearchStringValid(
                    requireComponents.core.client,
                    searchString
                )
            }

            when (result) {
                SearchStringValidator.Result.CannotReach -> {
                    custom_search_engine_search_string_field.error = resources
                        .getString(R.string.search_add_custom_engine_error_cannot_reach, name)
                }
                SearchStringValidator.Result.Success -> {
                    CustomSearchEngineStore.updateSearchEngine(
                        context = requireContext(),
                        oldEngineName = args.searchEngineIdentifier,
                        newEngineName = name,
                        searchQuery = searchString
                    )
                    requireComponents.search.provider.reload()
                    val successMessage = resources
                        .getString(R.string.search_edit_custom_engine_success_message, name)

                    view?.also {
                        FenixSnackbar.make(
                            view = it,
                            duration = FenixSnackbar.LENGTH_SHORT,
                            isDisplayedWithBrowserToolbar = false
                        )
                            .setText(successMessage)
                            .show()
                    }

                    findNavController().popBackStack()
                }
            }
        }
    }

    companion object {
        private const val DPS_TO_INCREASE = 20
    }
}
