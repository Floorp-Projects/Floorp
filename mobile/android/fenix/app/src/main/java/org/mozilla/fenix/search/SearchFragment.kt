/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import android.annotation.SuppressLint
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import androidx.navigation.Navigation
import kotlinx.android.synthetic.main.fragment_search.*
import kotlinx.android.synthetic.main.fragment_search.view.*
import mozilla.components.browser.domains.autocomplete.ShippedDomainsProvider
import org.mozilla.fenix.R
import org.mozilla.fenix.components.toolbar.ToolbarIntegration
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.mvi.ActionBusFactory

class SearchFragment : Fragment() {
    private lateinit var searchComponent: SearchComponent

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val view = inflater.inflate(R.layout.fragment_search, container, false)
        searchComponent = SearchComponent(view.toolbar_wrapper, ActionBusFactory.get(this),
            { v -> transitionToBrowser(v) })
        return view
    }

    override fun onResume() {
        super.onResume()
        searchComponent.editMode()
    }

    @SuppressLint("CheckResult")
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        layoutComponents(view.search_layout)

        lifecycle.addObserver(
            ToolbarIntegration(
                requireContext(),
                searchComponent.getView(),
                ShippedDomainsProvider().also { it.initialize(requireContext()) },
                requireComponents.core.historyStorage
            )
        )

        toolbar_wrapper.clipToOutline = false
    }

    private fun transitionToBrowser(toolbar: View) {
        Navigation.findNavController(toolbar)
            .navigate(R.id.action_searchFragment_to_browserFragment, null, null)
    }

    companion object {
        const val toolbarTextSizeSp = 14f
    }
}
