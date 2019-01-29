/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import androidx.core.view.ViewCompat
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.OnLifecycleEvent
import androidx.navigation.Navigation
import androidx.navigation.fragment.FragmentNavigator
import mozilla.components.browser.domains.autocomplete.DomainAutocompleteProvider
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.feature.toolbar.ToolbarAutocompleteFeature
import mozilla.components.feature.toolbar.ToolbarFeature
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.application
import org.mozilla.fenix.ext.components

class ToolbarIntegration(
    context: Context,
    toolbar: BrowserToolbar,
    domainAutocompleteProvider: DomainAutocompleteProvider,
    historyStorage: HistoryStorage,
    sessionId: String? = null
) : LifecycleObserver {
    init {
        toolbar.setMenuBuilder(context.components.toolbar.menuBuilder)

        val home = BrowserToolbar.Button(
            context.resources.getDrawable(
                R.drawable.ic_home,
                context.application.theme
            ), context.getString(R.string.browser_home_button)
        ) {
            Navigation.findNavController(toolbar).navigate(R.id.action_browserFragment_to_homeFragment)
        }

        toolbar.addBrowserAction(home)

        toolbar.onUrlClicked = {
            val extras = FragmentNavigator.Extras.Builder().addSharedElement(
                toolbar, ViewCompat.getTransitionName(toolbar)!!
            ).build()
            Navigation.findNavController(toolbar)
                .navigate(R.id.action_browserFragment_to_searchFragment, null, null, extras)
            false
        }

        ToolbarAutocompleteFeature(toolbar).apply {
            addDomainProvider(domainAutocompleteProvider)
            addHistoryStorageProvider(historyStorage)
        }
    }

    private val toolbarFeature: ToolbarFeature = ToolbarFeature(
        toolbar,
        context.components.core.sessionManager,
        context.components.useCases.sessionUseCases.loadUrl,
        { searchTerms -> context.components.useCases.searchUseCases.defaultSearch.invoke(searchTerms) },
        sessionId
    )

    @OnLifecycleEvent(Lifecycle.Event.ON_START)
    fun start() {
        toolbarFeature.start()
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_STOP)
    fun stop() {
        toolbarFeature.stop()
    }
}
