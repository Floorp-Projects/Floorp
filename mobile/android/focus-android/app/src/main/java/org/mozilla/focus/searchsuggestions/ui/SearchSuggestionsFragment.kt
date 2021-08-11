/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.searchsuggestions.ui

import android.os.Bundle
import android.text.method.LinkMovementMethod
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.compose.runtime.Composable
import androidx.compose.runtime.livedata.observeAsState
import androidx.compose.runtime.remember
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.LocalContext
import androidx.core.content.ContextCompat
import androidx.core.graphics.drawable.toBitmap
import androidx.fragment.app.Fragment
import androidx.lifecycle.Observer
import androidx.lifecycle.ViewModelProvider
import kotlinx.android.synthetic.main.fragment_search_suggestions.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import mozilla.components.compose.browser.awesomebar.AwesomeBar
import mozilla.components.feature.awesomebar.provider.SearchSuggestionProvider
import org.mozilla.focus.R
import org.mozilla.focus.components
import org.mozilla.focus.searchsuggestions.SearchSuggestionsViewModel
import org.mozilla.focus.searchsuggestions.State
import org.mozilla.focus.theme.FocusTheme
import kotlin.coroutines.CoroutineContext

class SearchSuggestionsFragment : Fragment(), CoroutineScope {
    private var job = Job()
    override val coroutineContext: CoroutineContext
        get() = job + Dispatchers.Main

    lateinit var searchSuggestionsViewModel: SearchSuggestionsViewModel

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

    @Suppress("DEPRECATION") // https://github.com/mozilla-mobile/focus-android/issues/4958
    override fun onActivityCreated(savedInstanceState: Bundle?) {
        super.onActivityCreated(savedInstanceState)

        searchSuggestionsViewModel = ViewModelProvider(requireActivity())
            .get(SearchSuggestionsViewModel::class.java)

        searchSuggestionsViewModel.state.observe(
            viewLifecycleOwner,
            Observer { state ->
                enable_search_suggestions_container.visibility = View.GONE
                no_suggestions_container.visibility = View.GONE

                when (state) {
                    is State.ReadyForSuggestions -> { /* Handled by Jetpack Compose implementation */
                    }
                    is State.NoSuggestionsAPI ->
                        no_suggestions_container.visibility = if (state.givePrompt) {
                            View.VISIBLE
                        } else {
                            View.GONE
                        }
                    is State.Disabled ->
                        enable_search_suggestions_container.visibility = if (state.givePrompt) {
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
    ): View? = inflater.inflate(R.layout.fragment_search_suggestions, container, false)

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        val appName = resources.getString(R.string.app_name)

        enable_search_suggestions_subtitle.text =
            resources.getString(R.string.enable_search_suggestion_description, appName)
        enable_search_suggestions_subtitle.movementMethod = LinkMovementMethod.getInstance()
        enable_search_suggestions_subtitle.highlightColor = android.graphics.Color.TRANSPARENT

        enable_search_suggestions_button.setOnClickListener {
            searchSuggestionsViewModel.enableSearchSuggestions()
        }

        disable_search_suggestions_button.setOnClickListener {
            searchSuggestionsViewModel.disableSearchSuggestions()
        }

        dismiss_no_suggestions_message.setOnClickListener {
            searchSuggestionsViewModel.dismissNoSuggestionsMessage()
        }

        val searchSuggestionsView = view.findViewById<ComposeView>(R.id.search_suggestions_view)
        searchSuggestionsView.setContent { SearchOverlay(searchSuggestionsViewModel) }
    }

    companion object {
        fun create() = SearchSuggestionsFragment()
    }
}

@Composable
private fun SearchOverlay(
    viewModel: SearchSuggestionsViewModel
) {
    val state = viewModel.state.observeAsState()

    when (state.value) {
        is State.Disabled -> { /* Not implemented in Compose, still handled by the View implementation */ }
        is State.NoSuggestionsAPI -> { /* Not implemented in Compose, still handled by the View implementation */ }
        State.ReadyForSuggestions -> SearchSuggestions(viewModel)
    }
}

@Composable
private fun SearchSuggestions(
    viewModel: SearchSuggestionsViewModel
) {
    val context = LocalContext.current
    val components = components
    val query = viewModel.searchQuery.observeAsState()

    val icon = ContextCompat.getDrawable(LocalContext.current, R.drawable.ic_search)
        ?.toBitmap()

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

    FocusTheme {
        AwesomeBar(
            text = query.value ?: "",
            providers = listOf(
                provider
            ),
            onSuggestionClicked = { suggestion ->
                viewModel.selectSearchSuggestion(
                    suggestion.title!!
                )
            },
            onAutoComplete = { suggestion ->
                val text = suggestion.editSuggestion ?: return@AwesomeBar
                viewModel.setAutocompleteSuggestion(text)
            }
        )
    }
}
