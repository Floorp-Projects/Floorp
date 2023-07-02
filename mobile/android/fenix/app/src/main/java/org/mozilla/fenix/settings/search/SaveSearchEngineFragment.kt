/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.search

import android.os.Bundle
import android.text.Editable
import android.text.TextWatcher
import android.view.View
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.browser.icons.IconRequest
import mozilla.components.feature.search.ext.createSearchEngine
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.databinding.FragmentSaveSearchEngineBinding
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.settings.SupportUtils

/**
 * A [Fragment] that allows user to add a new search engine.
 */
@SuppressWarnings("LargeClass", "TooManyFunctions")
class SaveSearchEngineFragment : Fragment(R.layout.fragment_save_search_engine) {

    private val args by navArgs<SaveSearchEngineFragmentArgs>()
    private val searchEngine by lazy {
        requireComponents.core.store.state.search.customSearchEngines.find { engine ->
            engine.id == args.searchEngineIdentifier
        }
    }

    private var _binding: FragmentSaveSearchEngineBinding? = null
    private val binding get() = _binding!!

    private val inputListener = object : TextWatcher {
        override fun afterTextChanged(editable: Editable) {
            val bothFieldsHaveInput = binding.editEngineName.text?.isNotBlank() == true &&
                binding.editSearchString.text?.isNotBlank() == true
            binding.saveButton.isEnabled = bothFieldsHaveInput
        }

        override fun beforeTextChanged(
            s: CharSequence?,
            start: Int,
            count: Int,
            after: Int,
        ) = Unit

        override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) =
            Unit
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        _binding = FragmentSaveSearchEngineBinding.bind(view)

        binding.saveButton.apply {
            isEnabled = false
            setOnClickListener { createCustomEngine() }
        }

        binding.editEngineName.addTextChangedListener(inputListener)
        binding.editSearchString.addTextChangedListener(inputListener)

        searchEngine?.let {
            val url = it.resultUrls[0]
            val suggestUrl = it.suggestUrl ?: ""
            binding.editEngineName.setText(it.name)
            binding.editSearchString.setText(url.toEditableUrl())
            binding.editSuggestString.setText(suggestUrl.toEditableUrl())
        }

        val learnMoreListener: (View) -> Unit = {
            (activity as HomeActivity).openToBrowserAndLoad(
                searchTermOrURL = SupportUtils.getSumoURLForTopic(
                    requireContext(),
                    SupportUtils.SumoTopic.CUSTOM_SEARCH_ENGINES,
                ),
                newTab = true,
                from = BrowserDirection.FromSaveSearchEngineFragment,
            )
        }
        binding.customSearchEnginesLearnMoreWrapper.setOnClickListener(learnMoreListener)
        binding.customSearchSuggestionsLearnMoreWrapper.setOnClickListener(learnMoreListener)
    }

    override fun onResume() {
        super.onResume()
        if (args.searchEngineIdentifier != null) {
            showToolbar(getString(R.string.search_engine_edit_custom_search_engine_title))
        } else {
            showToolbar(getString(R.string.search_engine_add_custom_search_engine_title))
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    @Suppress("ComplexMethod")
    private fun createCustomEngine() {
        binding.customSearchEngineNameField.error = ""
        binding.customSearchEngineSearchStringField.error = ""
        binding.customSearchEngineSuggestStringField.error = ""

        val name = binding.editEngineName.text?.toString()?.trim() ?: ""
        val searchString = binding.editSearchString.text?.toString() ?: ""
        val suggestString = binding.editSuggestString.text?.toString() ?: ""

        if (checkForErrors(name, searchString, suggestString)) {
            return
        }

        viewLifecycleOwner.lifecycleScope.launch(Main) {
            val searchStringResult = withContext(IO) {
                SearchStringValidator.isSearchStringValid(
                    requireComponents.core.client,
                    searchString,
                )
            }
            if (searchStringResult == SearchStringValidator.Result.CannotReach) {
                binding.customSearchEngineSearchStringField.error = resources
                    .getString(R.string.search_add_custom_engine_error_cannot_reach, name)
            }

            val suggestStringResult = if (suggestString.isBlank()) {
                SearchStringValidator.Result.Success
            } else {
                withContext(IO) {
                    SearchStringValidator.isSearchStringValid(
                        requireComponents.core.client,
                        suggestString,
                    )
                }
            }
            if (suggestStringResult == SearchStringValidator.Result.CannotReach) {
                binding.customSearchEngineSuggestStringField.error = resources
                    .getString(R.string.search_add_custom_engine_error_cannot_reach, name)
            }

            if ((searchStringResult == SearchStringValidator.Result.Success) &&
                (suggestStringResult == SearchStringValidator.Result.Success)
            ) {
                val update = searchEngine?.copy(
                    name = name,
                    resultUrls = listOf(searchString.toSearchUrl()),
                    icon = requireComponents.core.icons.loadIcon(IconRequest(searchString))
                        .await().bitmap,
                    suggestUrl = suggestString.toSearchUrl(),
                ) ?: run {
                    createSearchEngine(
                        name,
                        searchString.toSearchUrl(),
                        requireComponents.core.icons.loadIcon(IconRequest(searchString))
                            .await().bitmap,
                        suggestUrl = suggestString.toSearchUrl(),
                        isGeneral = true,
                    )
                }

                requireComponents.useCases.searchUseCases.addSearchEngine(update)

                val successMessage = if (searchEngine != null) {
                    resources.getString(R.string.search_edit_custom_engine_success_message, name)
                } else {
                    resources.getString(R.string.search_add_custom_engine_success_message, name)
                }

                view?.also {
                    FenixSnackbar.make(
                        view = it,
                        duration = FenixSnackbar.LENGTH_SHORT,
                        isDisplayedWithBrowserToolbar = false,
                    )
                        .setText(successMessage)
                        .show()
                }
                findNavController().popBackStack()
            }
        }
    }

    private fun checkForErrors(name: String, searchString: String, suggestString: String): Boolean {
        return when {
            name.isEmpty() -> {
                binding.customSearchEngineNameField.error = resources
                    .getString(R.string.search_add_custom_engine_error_empty_name)
                true
            }
            searchString.isEmpty() -> {
                binding.customSearchEngineSearchStringField.error =
                    resources.getString(R.string.search_add_custom_engine_error_empty_search_string)
                true
            }
            !searchString.contains("%s") -> {
                binding.customSearchEngineSearchStringField.error =
                    resources.getString(R.string.search_add_custom_engine_error_missing_template)
                true
            }
            suggestString.isNotBlank() && !suggestString.contains("%s") -> {
                binding.customSearchEngineSuggestStringField.error =
                    resources.getString(R.string.search_add_custom_engine_error_missing_template)
                true
            }
            else -> false
        }
    }
}

private fun String.toEditableUrl(): String {
    return replace("{searchTerms}", "%s")
}

private fun String.toSearchUrl(): String {
    return replace("%s", "{searchTerms}")
}
