/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.searchsuggestions.ui

import android.os.Bundle
import android.text.method.LinkMovementMethod
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.compose.ui.platform.ComposeView
import androidx.core.view.isVisible
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import mozilla.components.service.glean.private.NoExtras
import mozilla.components.support.ktx.android.view.hideKeyboard
import org.mozilla.focus.GleanMetrics.ShowSearchSuggestions
import org.mozilla.focus.R
import org.mozilla.focus.databinding.FragmentSearchSuggestionsBinding
import org.mozilla.focus.ext.defaultSearchEngineName
import org.mozilla.focus.ext.requireComponents
import org.mozilla.focus.searchsuggestions.SearchSuggestionsViewModel
import org.mozilla.focus.searchsuggestions.State
import org.mozilla.focus.ui.theme.FocusTheme
import kotlin.coroutines.CoroutineContext

class SearchSuggestionsFragment : Fragment(), CoroutineScope {
    private var job = Job()
    override val coroutineContext: CoroutineContext
        get() = job + Dispatchers.Main

    private var _binding: FragmentSearchSuggestionsBinding? = null
    private val binding get() = _binding!!

    private val defaultSearchEngineName: String
        get() = requireComponents.store.defaultSearchEngineName()

    private val searchSuggestionsViewModel: SearchSuggestionsViewModel by activityViewModels()

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

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        _binding = FragmentSearchSuggestionsBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        searchSuggestionsViewModel.state.observe(
            viewLifecycleOwner,
        ) { state ->
            binding.enableSearchSuggestionsContainer.isVisible = false
            binding.noSuggestionsContainer.isVisible = false

            when (state) {
                is State.ReadyForSuggestions -> { // Handled by Jetpack Compose implementation
                }
                is State.NoSuggestionsAPI ->
                    binding.noSuggestionsContainer.isVisible = state.givePrompt
                is State.Disabled ->
                    binding.enableSearchSuggestionsContainer.isVisible = state.givePrompt
            }
        }

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
            FocusTheme {
                SearchOverlay(
                    searchSuggestionsViewModel,
                    defaultSearchEngineName,
                ) { view.hideKeyboard() }
            }
        }
    }

    companion object {
        fun create() = SearchSuggestionsFragment()
    }
}
