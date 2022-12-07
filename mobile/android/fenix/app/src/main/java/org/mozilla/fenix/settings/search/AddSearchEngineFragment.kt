/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.search

import android.annotation.SuppressLint
import android.content.res.Resources
import android.graphics.drawable.BitmapDrawable
import android.os.Bundle
import android.view.LayoutInflater
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import android.widget.CompoundButton
import android.widget.LinearLayout
import android.widget.RadioButton
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.core.view.MenuProvider
import androidx.fragment.app.Fragment
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.browser.icons.IconRequest
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.availableSearchEngines
import mozilla.components.feature.search.ext.createSearchEngine
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.databinding.CustomSearchEngineBinding
import org.mozilla.fenix.databinding.CustomSearchEngineRadioButtonBinding
import org.mozilla.fenix.databinding.FragmentAddSearchEngineBinding
import org.mozilla.fenix.databinding.SearchEngineRadioButtonBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.settings.SupportUtils

@SuppressWarnings("LargeClass", "TooManyFunctions")
class AddSearchEngineFragment :
    Fragment(R.layout.fragment_add_search_engine),
    CompoundButton.OnCheckedChangeListener,
    MenuProvider {
    private var availableEngines: List<SearchEngine> = listOf()
    private var selectedIndex: Int = -1
    private val engineViews = mutableListOf<View>()

    private var _binding: FragmentAddSearchEngineBinding? = null
    private val binding get() = _binding!!
    private lateinit var customSearchEngine: CustomSearchEngineBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        availableEngines = requireContext()
            .components
            .core
            .store
            .state
            .search
            .availableSearchEngines

        selectedIndex = if (availableEngines.isEmpty()) CUSTOM_INDEX else FIRST_INDEX
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        requireActivity().addMenuProvider(this, viewLifecycleOwner, Lifecycle.State.RESUMED)

        val layoutInflater = LayoutInflater.from(context)
        val layoutParams = ViewGroup.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.WRAP_CONTENT,
        )
        _binding = FragmentAddSearchEngineBinding.bind(view)
        customSearchEngine = binding.customSearchEngine

        val setupSearchEngineItem: (Int, SearchEngine) -> Unit = { index, engine ->
            val engineId = engine.id
            val engineItem = makeButtonFromSearchEngine(
                engine = engine,
                layoutInflater = layoutInflater,
                res = requireContext().resources,
            )
            engineItem.root.id = index
            engineItem.root.tag = engineId
            engineItem.radioButton.isChecked = selectedIndex == index
            engineViews.add(engineItem.root)
            binding.searchEngineGroup.addView(engineItem.root, layoutParams)
        }

        availableEngines.forEachIndexed(setupSearchEngineItem)

        val engineItem = makeCustomButton(layoutInflater)
        engineItem.root.id = CUSTOM_INDEX
        engineItem.radioButton.isChecked = selectedIndex == CUSTOM_INDEX
        engineViews.add(engineItem.root)
        binding.searchEngineGroup.addView(engineItem.root, layoutParams)

        toggleCustomForm(selectedIndex == CUSTOM_INDEX)

        customSearchEngine.customSearchEnginesLearnMore.setOnClickListener {
            (activity as HomeActivity).openToBrowserAndLoad(
                searchTermOrURL = SupportUtils.getSumoURLForTopic(
                    requireContext(),
                    SupportUtils.SumoTopic.CUSTOM_SEARCH_ENGINES,
                ),
                newTab = true,
                from = BrowserDirection.FromAddSearchEngineFragment,
            )
        }
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.search_engine_add_custom_search_engine_title))
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    override fun onCreateMenu(menu: Menu, inflater: MenuInflater) {
        inflater.inflate(R.menu.add_custom_searchengine_menu, menu)
    }

    override fun onMenuItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            R.id.add_search_engine -> {
                when (selectedIndex) {
                    CUSTOM_INDEX -> createCustomEngine()
                    else -> {
                        val engine = availableEngines[selectedIndex]
                        requireComponents.useCases.searchUseCases.addSearchEngine(engine)
                        findNavController().popBackStack()
                    }
                }

                true
            }
            // other options are not handled by this menu provider
            else -> false
        }
    }

    @Suppress("ComplexMethod")
    private fun createCustomEngine() {
        customSearchEngine.customSearchEngineNameField.error = ""
        customSearchEngine.customSearchEngineSearchStringField.error = ""

        val name = customSearchEngine.editEngineName.text?.toString()?.trim() ?: ""
        val searchString = customSearchEngine.editSearchString.text?.toString() ?: ""

        if (checkForErrors(name, searchString)) {
            return
        }

        viewLifecycleOwner.lifecycleScope.launch(Main) {
            val result = withContext(IO) {
                SearchStringValidator.isSearchStringValid(
                    requireComponents.core.client,
                    searchString,
                )
            }

            when (result) {
                SearchStringValidator.Result.CannotReach -> {
                    customSearchEngine.customSearchEngineSearchStringField.error = resources
                        .getString(R.string.search_add_custom_engine_error_cannot_reach, name)
                }
                SearchStringValidator.Result.Success -> {
                    val searchEngine = createSearchEngine(
                        name,
                        searchString.toSearchUrl(),
                        requireComponents.core.icons.loadIcon(IconRequest(searchString)).await().bitmap,
                        isGeneral = true,
                    )

                    requireComponents.useCases.searchUseCases.addSearchEngine(searchEngine)

                    val successMessage = resources
                        .getString(R.string.search_add_custom_engine_success_message, name)

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
    }

    private fun checkForErrors(name: String, searchString: String): Boolean {
        return when {
            name.isEmpty() -> {
                customSearchEngine.customSearchEngineNameField.error = resources
                    .getString(R.string.search_add_custom_engine_error_empty_name)
                true
            }
            searchString.isEmpty() -> {
                customSearchEngine.customSearchEngineSearchStringField.error =
                    resources.getString(R.string.search_add_custom_engine_error_empty_search_string)
                true
            }
            !searchString.contains("%s") -> {
                customSearchEngine.customSearchEngineSearchStringField.error =
                    resources.getString(R.string.search_add_custom_engine_error_missing_template)
                true
            }
            else -> false
        }
    }

    override fun onCheckedChanged(buttonView: CompoundButton, isChecked: Boolean) {
        engineViews.forEach {
            when (it.findViewById<RadioButton>(R.id.radio_button) == buttonView) {
                true -> {
                    selectedIndex = it.id
                }
                false -> {
                    it.findViewById<RadioButton>(R.id.radio_button).also { radioButton ->
                        radioButton.setOnCheckedChangeListener(null)
                        radioButton.isChecked = false
                        radioButton.setOnCheckedChangeListener(this)
                    }
                }
            }
        }

        toggleCustomForm(selectedIndex == -1)
    }

    @SuppressLint("InflateParams")
    private fun makeCustomButton(layoutInflater: LayoutInflater): CustomSearchEngineRadioButtonBinding {
        val wrapper = layoutInflater
            .inflate(R.layout.custom_search_engine_radio_button, null) as ConstraintLayout
        val customSearchEngineRadioButtonBinding = CustomSearchEngineRadioButtonBinding.bind(wrapper)
        wrapper.setOnClickListener { customSearchEngineRadioButtonBinding.radioButton.isChecked = true }
        customSearchEngineRadioButtonBinding.radioButton.setOnCheckedChangeListener(this)
        return customSearchEngineRadioButtonBinding
    }

    private fun toggleCustomForm(isEnabled: Boolean) {
        customSearchEngine.customSearchEngineForm.alpha = if (isEnabled) ENABLED_ALPHA else DISABLED_ALPHA
        customSearchEngine.editSearchString.isEnabled = isEnabled
        customSearchEngine.editEngineName.isEnabled = isEnabled
        customSearchEngine.customSearchEnginesLearnMore.isEnabled = isEnabled
    }

    @SuppressLint("InflateParams")
    private fun makeButtonFromSearchEngine(
        engine: SearchEngine,
        layoutInflater: LayoutInflater,
        res: Resources,
    ): SearchEngineRadioButtonBinding {
        val wrapper = layoutInflater
            .inflate(R.layout.search_engine_radio_button, null) as LinearLayout
        val searchEngineRadioButtonBinding = SearchEngineRadioButtonBinding.bind(wrapper)

        wrapper.setOnClickListener { searchEngineRadioButtonBinding.radioButton.isChecked = true }
        searchEngineRadioButtonBinding.radioButton.setOnCheckedChangeListener(this)
        searchEngineRadioButtonBinding.engineText.text = engine.name
        val iconSize = res.getDimension(R.dimen.preference_icon_drawable_size).toInt()
        val engineIcon = BitmapDrawable(res, engine.icon)
        engineIcon.setBounds(0, 0, iconSize, iconSize)
        searchEngineRadioButtonBinding.engineIcon.setImageDrawable(engineIcon)
        searchEngineRadioButtonBinding.overflowMenu.visibility = View.GONE
        return searchEngineRadioButtonBinding
    }

    companion object {
        private const val ENABLED_ALPHA = 1.0f
        private const val DISABLED_ALPHA = 0.2f
        private const val CUSTOM_INDEX = -1
        private const val FIRST_INDEX = 0
    }
}

private fun String.toSearchUrl(): String {
    return replace("%s", "{searchTerms}")
}
