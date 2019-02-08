/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.toolbar

import android.content.Context
import androidx.navigation.Navigation
import mozilla.components.browser.domains.autocomplete.DomainAutocompleteProvider
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.runWithSession
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.feature.toolbar.ToolbarAutocompleteFeature
import mozilla.components.feature.toolbar.ToolbarFeature
import org.mozilla.fenix.DefaultThemeManager
import mozilla.components.support.base.feature.LifecycleAwareFeature
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.application
import org.mozilla.fenix.ext.components

class ToolbarIntegration(
    context: Context,
    toolbar: BrowserToolbar,
    toolbarMenu: ToolbarMenu,
    domainAutocompleteProvider: DomainAutocompleteProvider,
    historyStorage: HistoryStorage,
    sessionManager: SessionManager,
    sessionId: String? = null
) : LifecycleAwareFeature {
    init {
        toolbar.setMenuBuilder(toolbarMenu.menuBuilder)

        val home = BrowserToolbar.Button(
            context.resources
                .getDrawable(DefaultThemeManager.resolveAttribute(R.attr.browserToolbarHomeIcon, context),
                    context.application.theme),
            context.getString(R.string.browser_home_button),
            visible = { sessionManager.runWithSession(sessionId) { it.isCustomTabSession().not() } }
        ) {
            Navigation.findNavController(toolbar).navigate(R.id.action_browserFragment_to_homeFragment)
        }

        toolbar.addBrowserAction(home)

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

    override fun start() {
        toolbarFeature.start()
    }

    override fun stop() {
        toolbarFeature.stop()
    }
}
