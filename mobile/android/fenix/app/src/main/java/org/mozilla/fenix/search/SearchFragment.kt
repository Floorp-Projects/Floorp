/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import android.content.Context
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import kotlinx.android.synthetic.main.fragment_search.view.*
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.session.SessionUseCases
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.toolbar.SearchAction
import org.mozilla.fenix.components.toolbar.SearchState
import org.mozilla.fenix.components.toolbar.ToolbarComponent
import org.mozilla.fenix.components.toolbar.ToolbarUIView
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.getAutoDisposeObservable
import org.mozilla.fenix.mvi.getManagedEmitter
import org.mozilla.fenix.search.awesomebar.AwesomeBarAction
import org.mozilla.fenix.search.awesomebar.AwesomeBarChange
import org.mozilla.fenix.search.awesomebar.AwesomeBarComponent

class SearchFragment : Fragment() {
    private lateinit var toolbarComponent: ToolbarComponent
    private lateinit var awesomeBarComponent: AwesomeBarComponent
    private var sessionId: String? = null
    private var isPrivate = false

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        sessionId = SearchFragmentArgs.fromBundle(arguments!!).sessionId
        isPrivate = (activity as HomeActivity).browsingModeManager.isPrivate
        val view = inflater.inflate(R.layout.fragment_search, container, false)
        val url = sessionId?.let {
            requireComponents.core.sessionManager.findSessionById(it)?.let { session ->
                session.url
            }
        } ?: ""

        toolbarComponent = ToolbarComponent(
            view.toolbar_wrapper,
            ActionBusFactory.get(this),
            sessionId,
            isPrivate,
            SearchState(url, isEditing = true)
        )
        awesomeBarComponent = AwesomeBarComponent(view.search_layout, ActionBusFactory.get(this))
        ActionBusFactory.get(this).logMergedObservables()
        return view
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        layoutComponents(view.search_layout)

        lifecycle.addObserver((toolbarComponent.uiView as ToolbarUIView).toolbarIntegration)

        view.toolbar_wrapper.clipToOutline = false
    }

    override fun onResume() {
        super.onResume()
        (activity as AppCompatActivity).supportActionBar?.hide()
    }

    override fun onStart() {
        super.onStart()
        getAutoDisposeObservable<SearchAction>()
            .subscribe {
                when (it) {
                    is SearchAction.UrlCommitted -> {
                        if (it.url.isNotBlank()) {
                            (activity as HomeActivity).openToBrowserAndLoad(it.url, it.session,
                                BrowserDirection.FromSearch)
                        }
                    }
                    is SearchAction.TextChanged -> {
                        getManagedEmitter<AwesomeBarChange>().onNext(AwesomeBarChange.UpdateQuery(it.query))
                    }
                }
            }

        getAutoDisposeObservable<AwesomeBarAction>()
            .subscribe {
                when (it) {
                    is AwesomeBarAction.URLTapped -> {
                        getSessionUseCase(requireContext(), sessionId == null).invoke(it.url)
                        (activity as HomeActivity).openToBrowser(sessionId, BrowserDirection.FromSearch)
                    }
                    is AwesomeBarAction.SearchTermsTapped -> {
                        getSearchUseCase(requireContext(), sessionId == null)
                            .invoke(it.searchTerms, it.engine)
                        (activity as HomeActivity).openToBrowser(sessionId, BrowserDirection.FromSearch)
                    }
                }
            }
    }

    private fun getSearchUseCase(context: Context, useNewTab: Boolean): SearchUseCases.SearchUseCase {
        if (!useNewTab) {
            return context.components.useCases.searchUseCases.defaultSearch
        }

        return when (isPrivate) {
            true -> context.components.useCases.searchUseCases.newPrivateTabSearch
            false -> context.components.useCases.searchUseCases.newTabSearch
        }
    }

    private fun getSessionUseCase(context: Context, useNewTab: Boolean): SessionUseCases.LoadUrlUseCase {
        if (!useNewTab) {
            return context.components.useCases.sessionUseCases.loadUrl
        }

        return when (isPrivate) {
            true -> context.components.useCases.tabsUseCases.addPrivateTab
            false -> context.components.useCases.tabsUseCases.addTab
        }
    }
}
