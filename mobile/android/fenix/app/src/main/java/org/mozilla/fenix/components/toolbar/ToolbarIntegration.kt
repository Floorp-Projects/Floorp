/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import androidx.navigation.Navigation
import mozilla.components.browser.domains.autocomplete.DomainAutocompleteProvider
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.runWithSession
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.feature.toolbar.ToolbarAutocompleteFeature
import mozilla.components.feature.toolbar.ToolbarPresenter
import mozilla.components.support.base.feature.LifecycleAwareFeature
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.ext.components

class ToolbarIntegration(
    context: Context,
    toolbar: BrowserToolbar,
    toolbarMenu: ToolbarMenu,
    domainAutocompleteProvider: DomainAutocompleteProvider,
    historyStorage: HistoryStorage,
    sessionManager: SessionManager,
    sessionId: String? = null,
    isPrivate: Boolean
) : LifecycleAwareFeature {
    init {
        toolbar.setMenuBuilder(toolbarMenu.menuBuilder)
        toolbar.private = isPrivate

        run {
            sessionManager.runWithSession(sessionId) {
                it.isCustomTabSession()
            }.also { isCustomTab ->
                if (isCustomTab) return@run

                val tabsAction = TabCounterToolbarButton(
                    sessionManager,
                    showTabs = {
                        Navigation.findNavController(toolbar)
                            .navigate(BrowserFragmentDirections.actionBrowserFragmentToHomeFragment())
                    }
                )
                toolbar.addBrowserAction(tabsAction)
            }
        }

        ToolbarAutocompleteFeature(toolbar).apply {
            addDomainProvider(domainAutocompleteProvider)
            addHistoryStorageProvider(historyStorage)
        }
    }

    private val toolbarPresenter: ToolbarPresenter = ToolbarPresenter(
        toolbar,
        context.components.core.sessionManager,
        sessionId
    )

    override fun start() {
        toolbarPresenter.start()
    }

    override fun stop() {
        toolbarPresenter.stop()
    }
}
