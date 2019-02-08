/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import android.annotation.SuppressLint
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import androidx.navigation.Navigation
import kotlinx.android.synthetic.main.fragment_search.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.getManagedEmitter
import org.mozilla.fenix.mvi.getSafeManagedObservable
import org.mozilla.fenix.search.awesomebar.AwesomeBarAction
import org.mozilla.fenix.search.awesomebar.AwesomeBarChange
import org.mozilla.fenix.search.awesomebar.AwesomeBarComponent
import org.mozilla.fenix.search.awesomebar.AwesomeBarState
import org.mozilla.fenix.search.toolbar.SearchAction
import org.mozilla.fenix.search.toolbar.SearchState
import org.mozilla.fenix.search.toolbar.ToolbarComponent
import org.mozilla.fenix.search.toolbar.ToolbarUIView

class SearchFragment : Fragment() {
    private lateinit var toolbarComponent: ToolbarComponent
    private lateinit var awesomeBarComponent: AwesomeBarComponent

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val sessionId = SearchFragmentArgs.fromBundle(arguments!!).sessionId
        val view = inflater.inflate(R.layout.fragment_search, container, false)
        toolbarComponent = ToolbarComponent(
            view.toolbar_wrapper,
            ActionBusFactory.get(this),
            sessionId,
            SearchState("", isEditing = true)
        )
        awesomeBarComponent = AwesomeBarComponent(
            view.search_layout, ActionBusFactory.get(this),
            AwesomeBarState("", sessionId == null)
        )
        ActionBusFactory.get(this).logMergedObservables()
        return view
    }

    @SuppressLint("CheckResult")
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        (activity as AppCompatActivity).supportActionBar?.hide()

        layoutComponents(view.search_layout)

        lifecycle.addObserver((toolbarComponent.uiView as ToolbarUIView).toolbarIntegration)

        view.toolbar_wrapper.clipToOutline = false

        getSafeManagedObservable<SearchAction>()
            .subscribe {
                when (it) {
                    is SearchAction.UrlCommitted -> transitionToBrowser()
                    is SearchAction.TextChanged -> {
                        getManagedEmitter<AwesomeBarChange>().onNext(AwesomeBarChange.UpdateQuery(it.query))
                    }
                }
            }

        getSafeManagedObservable<AwesomeBarAction>()
            .subscribe {
                when (it) {
                    is AwesomeBarAction.ItemSelected -> transitionToBrowser()
                }
            }
    }

    private fun transitionToBrowser() {
        val sessionId = SearchFragmentArgs.fromBundle(arguments!!).sessionId
        val directions = SearchFragmentDirections.actionSearchFragmentToBrowserFragment(sessionId)
        Navigation.findNavController(view!!.search_layout).navigate(directions)
    }
}
