/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import android.graphics.PorterDuff
import androidx.core.content.ContextCompat
import androidx.navigation.Navigation
import mozilla.components.browser.domains.autocomplete.DomainAutocompleteProvider
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.runWithSession
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.feature.toolbar.ToolbarAutocompleteFeature
import mozilla.components.feature.toolbar.ToolbarFeature
import mozilla.components.support.base.feature.LifecycleAwareFeature
import org.mozilla.fenix.DefaultThemeManager
import org.mozilla.fenix.R
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

        val tabsIcon = context.getDrawable(R.drawable.ic_tabs)
        tabsIcon?.setColorFilter(
            ContextCompat.getColor(
                context,
                DefaultThemeManager.resolveAttribute(R.attr.browserToolbarIcons, context)
            ), PorterDuff.Mode.SRC_IN
        )
        tabsIcon?.let {
            val home = BrowserToolbar.Button(
                it,
                context.getString(R.string.browser_home_button),
                visible = {
                    sessionId == null ||
                            sessionManager.runWithSession(sessionId) {
                                it.isCustomTabSession().not()
                            }
                }
            ) {
                Navigation.findNavController(toolbar)
                    .navigate(BrowserFragmentDirections.actionBrowserFragmentToHomeFragment())
            }

            toolbar.addBrowserAction(home)
        }

        ToolbarAutocompleteFeature(toolbar).apply {
            addDomainProvider(domainAutocompleteProvider)
            addHistoryStorageProvider(historyStorage)
        }
    }

    private val toolbarFeature: ToolbarFeature = ToolbarFeature(
        toolbar,
        context.components.core.sessionManager,
        if (sessionId == null) {
            if (isPrivate) {
                context.components.useCases.tabsUseCases.addPrivateTab
            } else {
                context.components.useCases.tabsUseCases.addTab
            }
        } else context.components.useCases.sessionUseCases.loadUrl,
        { searchTerms -> if (sessionId == null) {
            context.components.useCases.searchUseCases.newTabSearch
                .invoke(searchTerms, Session.Source.USER_ENTERED, true, isPrivate)
        } else context.components.useCases.searchUseCases.defaultSearch.invoke(searchTerms) },
        sessionId
    )

    override fun start() {
        toolbarFeature.start()
    }

    override fun stop() {
        toolbarFeature.stop()
    }
}
