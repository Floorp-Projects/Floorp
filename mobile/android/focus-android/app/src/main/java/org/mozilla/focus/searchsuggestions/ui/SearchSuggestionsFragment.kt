/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.searchsuggestions.ui

import android.os.Bundle
import android.text.method.LinkMovementMethod
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.content.res.AppCompatResources
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.runtime.Composable
import androidx.compose.runtime.livedata.observeAsState
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.input.nestedscroll.NestedScrollConnection
import androidx.compose.ui.input.nestedscroll.NestedScrollSource
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.core.graphics.drawable.toBitmap
import androidx.fragment.app.Fragment
import androidx.lifecycle.Observer
import androidx.lifecycle.ViewModelProvider
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import mozilla.components.compose.browser.awesomebar.AwesomeBar
import mozilla.components.compose.browser.awesomebar.AwesomeBarDefaults
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.feature.awesomebar.provider.SearchSuggestionProvider
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.lib.state.ext.observeAsComposableState
import mozilla.components.service.glean.private.NoExtras
import mozilla.components.support.ktx.android.view.hideKeyboard
import org.mozilla.focus.GleanMetrics.ShowSearchSuggestions
import org.mozilla.focus.R
import org.mozilla.focus.components
import org.mozilla.focus.databinding.FragmentSearchSuggestionsBinding
import org.mozilla.focus.ext.defaultSearchEngineName
import org.mozilla.focus.ext.requireComponents
import org.mozilla.focus.searchsuggestions.SearchSuggestionsViewModel
import org.mozilla.focus.searchsuggestions.State
import org.mozilla.focus.topsites.TopSites
import org.mozilla.focus.ui.theme.FocusTheme
import org.mozilla.focus.ui.theme.focusColors
import kotlin.coroutines.CoroutineContext

class SearchSuggestionsFragment : Fragment(), CoroutineScope {
    private var job = Job()
    override val coroutineContext: CoroutineContext
        get() = job + Dispatchers.Main

    private var _binding: FragmentSearchSuggestionsBinding? = null
    private val binding get() = _binding!!
    lateinit var searchSuggestionsViewModel: SearchSuggestionsViewModel

    private val defaultSearchEngineName: String
        get() = requireComponents.store.defaultSearchEngineName()

    override fun onResume() {
        super.onResume()

        if (job.isCancelled) {
            job = Job()
        }

        searchSuggestionsViewModel.refresh()
    }

    override fun onPause() {
        job.cancel()
        super.onPause()
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    @Suppress("DEPRECATION") // https://github.com/mozilla-mobile/focus-android/issues/4958
    override fun onActivityCreated(savedInstanceState: Bundle?) {
        super.onActivityCreated(savedInstanceState)

        searchSuggestionsViewModel = ViewModelProvider(requireParentFragment())
            .get(SearchSuggestionsViewModel::class.java)

        searchSuggestionsViewModel.state.observe(
            viewLifecycleOwner,
            Observer { state ->
                binding.enableSearchSuggestionsContainer.visibility = View.GONE
                binding.noSuggestionsContainer.visibility = View.GONE

                when (state) {
                    is State.ReadyForSuggestions -> { /* Handled by Jetpack Compose implementation */
                    }
                    is State.NoSuggestionsAPI ->
                        binding.noSuggestionsContainer.visibility = if (state.givePrompt) {
                            View.VISIBLE
                        } else {
                            View.GONE
                        }
                    is State.Disabled ->
                        binding.enableSearchSuggestionsContainer.visibility =
                            if (state.givePrompt) {
                                View.VISIBLE
                            } else {
                                View.GONE
                            }
                }
            }
        )
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentSearchSuggestionsBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        val appName = resources.getString(R.string.app_name)

        binding.enableSearchSuggestionsSubtitle.apply {
            text = resources.getString(R.string.enable_search_suggestion_description, appName)
            movementMethod = LinkMovementMethod.getInstance()
            highlightColor = android.graphics.Color.TRANSPARENT
        }
        binding.enableSearchSuggestionsButton.setOnClickListener {
            searchSuggestionsViewModel.enableSearchSuggestions()
            ShowSearchSuggestions.enabledFromPanel.record(NoExtras())
        }

        binding.disableSearchSuggestionsButton.setOnClickListener {
            searchSuggestionsViewModel.disableSearchSuggestions()
            ShowSearchSuggestions.disabledFromPanel.record(NoExtras())
        }

        binding.dismissNoSuggestionsMessage.setOnClickListener {
            searchSuggestionsViewModel.dismissNoSuggestionsMessage()
        }

        val searchSuggestionsView = view.findViewById<ComposeView>(R.id.search_suggestions_view)
        searchSuggestionsView.setContent {
            SearchOverlay(
                searchSuggestionsViewModel,
                defaultSearchEngineName
            ) { view.hideKeyboard() }
        }
    }

    companion object {
        fun create() = SearchSuggestionsFragment()
    }
}

@OptIn(DelicateCoroutinesApi::class)
@Composable
private fun SearchOverlay(
    viewModel: SearchSuggestionsViewModel,
    defaultSearchEngineName: String,
    onListScrolled: () -> Unit
) {
    val topSitesState =
        components.appStore.observeAsComposableState { state -> state.topSites }
    val state = viewModel.state.observeAsState()
    val query = viewModel.searchQuery.observeAsState()

    val topSites = topSitesState.value!!

    when (state.value) {
        is State.Disabled,
        is State.NoSuggestionsAPI -> {
            if (query.value.isNullOrEmpty() && topSites.isNotEmpty()) {
                TopSitesOverlay(topSites = topSites)
            }
        }
        is State.ReadyForSuggestions -> {
            if (query.value.isNullOrEmpty() && topSites.isNotEmpty()) {
                TopSitesOverlay(topSites = topSites)
            } else {
                SearchSuggestions(
                    text = query.value ?: "",
                    onSuggestionClicked = { suggestion ->
                        viewModel.selectSearchSuggestion(
                            suggestion.title!!,
                            defaultSearchEngineName
                        )
                    },
                    onAutoComplete = { suggestion ->
                        val editSuggestion = suggestion.editSuggestion ?: return@SearchSuggestions
                        viewModel.setAutocompleteSuggestion(editSuggestion)
                    },
                    onListScrolled = onListScrolled
                )
            }
        }
    }
}

@Composable
private fun SearchSuggestions(
    text: String,
    onSuggestionClicked: (AwesomeBar.Suggestion) -> Unit,
    onAutoComplete: (AwesomeBar.Suggestion) -> Unit,
    onListScrolled: () -> Unit
) {
    val context = LocalContext.current
    val components = components

    val icon = AppCompatResources.getDrawable(context, R.drawable.mozac_ic_search)?.toBitmap()
    val provider = remember(context) {
        SearchSuggestionProvider(
            context,
            components.store,
            components.searchUseCases.newPrivateTabSearch,
            components.client,
            mode = SearchSuggestionProvider.Mode.MULTIPLE_SUGGESTIONS,
            private = true,
            showDescription = false,
            icon = icon
        )
    }

    val nestedScrollConnection = remember {
        object : NestedScrollConnection {
            override fun onPreScroll(available: Offset, source: NestedScrollSource): Offset {
                onListScrolled.invoke()
                return Offset.Zero
            }
        }
    }

    FocusTheme {
        Column(
            modifier = Modifier.nestedScroll(nestedScrollConnection)
        ) {
            AwesomeBar(
                text = text,
                colors = AwesomeBarDefaults.colors(
                    background = focusColors.surface
                ),
                providers = listOf(provider),
                onSuggestionClicked = onSuggestionClicked,
                onAutoComplete = onAutoComplete
            )
        }
    }
}

@Composable
private fun TopSitesOverlay(
    topSites: List<TopSite>
) {
    FocusTheme {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .background(focusColors.surface),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Spacer(modifier = Modifier.height(24.dp))

            TopSites(topSites = topSites)

            Spacer(modifier = Modifier.height(32.dp))
        }
    }
}
