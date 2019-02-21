/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import androidx.navigation.Navigation
import kotlinx.android.synthetic.main.fragment_search.view.*
import mozilla.components.browser.session.Session
import mozilla.components.support.ktx.kotlin.isUrl
import mozilla.components.support.ktx.kotlin.toNormalizedUrl
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.toolbar.SearchAction
import org.mozilla.fenix.components.toolbar.SearchState
import org.mozilla.fenix.components.toolbar.ToolbarComponent
import org.mozilla.fenix.components.toolbar.ToolbarUIView
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.getAutoDisposeObservable
import org.mozilla.fenix.mvi.getManagedEmitter
import org.mozilla.fenix.search.awesomebar.AwesomeBarAction
import org.mozilla.fenix.search.awesomebar.AwesomeBarChange
import org.mozilla.fenix.search.awesomebar.AwesomeBarComponent
import org.mozilla.fenix.search.awesomebar.AwesomeBarState

class SearchFragment : Fragment() {
    private lateinit var toolbarComponent: ToolbarComponent
    private lateinit var awesomeBarComponent: AwesomeBarComponent

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val sessionId = SearchFragmentArgs.fromBundle(arguments!!).sessionId
        val isPrivate = SearchFragmentArgs.fromBundle(arguments!!).isPrivateTab
        val view = inflater.inflate(R.layout.fragment_search, container, false)
        val url = sessionId?.let {
            requireComponents.core.sessionManager.findSessionById(it)?.let {
                    session -> session.url
            }
        } ?: ""

        toolbarComponent = ToolbarComponent(
            view.toolbar_wrapper,
            ActionBusFactory.get(this),
            sessionId,
            isPrivate,
            SearchState(url, isEditing = true)
        )
        awesomeBarComponent = AwesomeBarComponent(
            view.search_layout, ActionBusFactory.get(this),
            AwesomeBarState("", sessionId == null)
        )
        ActionBusFactory.get(this).logMergedObservables()
        return view
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        (activity as AppCompatActivity).supportActionBar?.hide()

        layoutComponents(view.search_layout)

        lifecycle.addObserver((toolbarComponent.uiView as ToolbarUIView).toolbarIntegration)

        view.toolbar_wrapper.clipToOutline = false
    }

    override fun onStart() {
        super.onStart()
        getAutoDisposeObservable<SearchAction>()
            .subscribe {
                when (it) {
                    is SearchAction.UrlCommitted -> {
                        transitionToBrowser()
                        load(it.url)
                    }
                    is SearchAction.TextChanged -> {
                        getManagedEmitter<AwesomeBarChange>().onNext(AwesomeBarChange.UpdateQuery(it.query))
                    }
                }
            }

        getAutoDisposeObservable<AwesomeBarAction>()
            .subscribe {
                when (it) {
                    is AwesomeBarAction.ItemSelected -> transitionToBrowser()
                }
            }
    }

    // Issue: https://github.com/mozilla-mobile/fenix/issues/626
    // Currently we were kind of forcing all this logic through the Toolbar Feature.
    // But since we cannot actually load a page without an available GeckoSession
    // we have to wait until after we navigate to call the use case.
    // We should move this logic into a place that makes more sense.
    private fun load(text: String) {
        val sessionId = SearchFragmentArgs.fromBundle(arguments!!).sessionId
        val isPrivate = SearchFragmentArgs.fromBundle(arguments!!).isPrivateTab

        val loadUrlUseCase = if (sessionId == null) {
            if (isPrivate) {
                requireComponents.useCases.tabsUseCases.addPrivateTab
            } else {
                requireComponents.useCases.tabsUseCases.addTab
            }
        } else requireComponents.useCases.sessionUseCases.loadUrl

        val searchUseCase: (String) -> Unit = { searchTerms ->
            if (sessionId == null) {
                requireComponents.useCases.searchUseCases.newTabSearch
                    .invoke(searchTerms, Session.Source.USER_ENTERED, true, isPrivate)
            } else requireComponents.useCases.searchUseCases.defaultSearch.invoke(searchTerms)
        }

        if (text.isUrl()) {
            loadUrlUseCase.invoke(text.toNormalizedUrl())
        } else {
            searchUseCase.invoke(text)
        }
    }

    private fun transitionToBrowser() {
        val sessionId = SearchFragmentArgs.fromBundle(arguments!!).sessionId
        val directions = SearchFragmentDirections.actionSearchFragmentToBrowserFragment(sessionId,
            (activity as HomeActivity).browsingModeManager.isPrivate)

        Navigation.findNavController(view!!.search_layout).navigate(directions)
    }
}
