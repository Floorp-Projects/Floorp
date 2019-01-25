/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.fenix.search

import android.os.Bundle
import android.transition.TransitionInflater
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.content.ContextCompat
import androidx.core.view.ViewCompat
import androidx.fragment.app.Fragment
import androidx.navigation.Navigation
import androidx.navigation.fragment.FragmentNavigator
import kotlinx.android.synthetic.main.fragment_search.*
import mozilla.components.browser.domains.autocomplete.ShippedDomainsProvider
import mozilla.components.feature.awesomebar.AwesomeBarFeature
import mozilla.components.feature.awesomebar.provider.SearchSuggestionProvider
import org.mozilla.fenix.R
import org.mozilla.fenix.components.toolbar.ToolbarIntegration
import org.mozilla.fenix.ext.requireComponents

class SearchFragment : Fragment() {
    private lateinit var awesomeBarFeature: AwesomeBarFeature

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        return inflater.inflate(R.layout.fragment_search, container, false)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        sharedElementEnterTransition = TransitionInflater.from(context).inflateTransition(android.R.transition.move)
    }

    override fun onResume() {
        super.onResume()
        toolbar.editMode()
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        lifecycle.addObserver(
            ToolbarIntegration(
            requireContext(),
            toolbar,
            ShippedDomainsProvider().also { it.initialize(requireContext()) })
        )

        awesomeBarFeature = AwesomeBarFeature(awesomeBar, toolbar, null, onEditComplete = ::userDidSearch)
            .addClipboardProvider(requireContext(), requireComponents.useCases.sessionUseCases.loadUrl)
            .addSearchProvider(
                requireComponents.search.searchEngineManager.getDefaultSearchEngine(requireContext()),
                requireComponents.useCases.searchUseCases.defaultSearch,
                SearchSuggestionProvider.Mode.MULTIPLE_SUGGESTIONS)
            .addSessionProvider(
                requireComponents.core.sessionManager,
                requireComponents.useCases.tabsUseCases.selectTab)


        toolbar_wrapper.clipToOutline = false
        toolbar.apply {
            textColor = ContextCompat.getColor(context, R.color.searchText)
            textSize = 14f
            hint = context.getString(R.string.search_hint)
            hintColor = ContextCompat.getColor(context, R.color.searchText)
        }
    }

    private fun userDidSearch() {
        val extras = FragmentNavigator.Extras.Builder().addSharedElement(
            toolbar, ViewCompat.getTransitionName(toolbar)!!
        ).build()
        Navigation.findNavController(toolbar).navigate(R.id.action_searchFragment_to_browserFragment, null, null, extras)
    }
}
