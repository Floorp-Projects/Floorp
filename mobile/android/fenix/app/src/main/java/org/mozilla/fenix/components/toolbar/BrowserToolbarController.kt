/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import androidx.navigation.NavController
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineView
import mozilla.components.support.ktx.kotlin.isUrl
import mozilla.components.ui.tabcounter.TabCounterMenu
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserAnimator.Companion.getToolbarNavOptions
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.readermode.ReaderModeController
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.sessionsOfType
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.home.HomeScreenViewModel

/**
 * An interface that handles the view manipulation of the BrowserToolbar, triggered by the Interactor
 */
interface BrowserToolbarController {
    fun handleScroll(offset: Int)
    fun handleToolbarPaste(text: String)
    fun handleToolbarPasteAndGo(text: String)
    fun handleToolbarClick()
    fun handleTabCounterClick()
    fun handleTabCounterItemInteraction(item: TabCounterMenu.Item)
    fun handleReaderModePressed(enabled: Boolean)
}

class DefaultBrowserToolbarController(
    private val store: BrowserStore,
    private val activity: HomeActivity,
    private val navController: NavController,
    private val metrics: MetricController,
    private val readerModeController: ReaderModeController,
    private val sessionManager: SessionManager,
    private val engineView: EngineView,
    private val homeViewModel: HomeScreenViewModel,
    private val customTabSession: Session?,
    private val onTabCounterClicked: () -> Unit,
    private val onCloseTab: (Session) -> Unit
) : BrowserToolbarController {

    private val currentSession
        get() = customTabSession ?: sessionManager.selectedSession

    override fun handleToolbarPaste(text: String) {
        navController.nav(
            R.id.browserFragment,
            BrowserFragmentDirections.actionGlobalSearchDialog(
                sessionId = currentSession?.id,
                pastedText = text
            ),
            getToolbarNavOptions(activity)
        )
    }

    override fun handleToolbarPasteAndGo(text: String) {
        if (text.isUrl()) {
            store.updateSearchTermsOfSelectedSession("")
            activity.components.useCases.sessionUseCases.loadUrl.invoke(text)
            return
        }

        store.updateSearchTermsOfSelectedSession(text)
        activity.components.useCases.searchUseCases.defaultSearch.invoke(
            text,
            sessionId = store.state.selectedTabId
        )
    }

    override fun handleToolbarClick() {
        metrics.track(Event.SearchBarTapped(Event.SearchBarTapped.Source.BROWSER))
        navController.nav(
            R.id.browserFragment,
            BrowserFragmentDirections.actionGlobalSearchDialog(
                currentSession?.id
            ),
            getToolbarNavOptions(activity)
        )
    }

    override fun handleTabCounterClick() {
        onTabCounterClicked.invoke()
    }

    override fun handleReaderModePressed(enabled: Boolean) {
        if (enabled) {
            readerModeController.showReaderView()
            metrics.track(Event.ReaderModeOpened)
        } else {
            readerModeController.hideReaderView()
            metrics.track(Event.ReaderModeClosed)
        }
    }

    override fun handleTabCounterItemInteraction(item: TabCounterMenu.Item) {
        when (item) {
            is TabCounterMenu.Item.CloseTab -> {
                metrics.track(
                    Event.TabCounterMenuItemTapped(Event.TabCounterMenuItemTapped.Item.CLOSE_TAB)
                )
                sessionManager.selectedSession?.let {
                    // When closing the last tab we must show the undo snackbar in the home fragment
                    if (sessionManager.sessionsOfType(it.private).count() == 1) {
                        homeViewModel.sessionToDelete = it.id
                        navController.navigate(
                            BrowserFragmentDirections.actionGlobalHome()
                        )
                    } else {
                        onCloseTab.invoke(it)
                        // The removeTab use case does not currently select a parent session, so
                        // we are using sessionManager.remove
                        sessionManager.remove(it, selectParentIfExists = true)
                    }
                }
            }
            is TabCounterMenu.Item.NewTab -> {
                metrics.track(
                    Event.TabCounterMenuItemTapped(Event.TabCounterMenuItemTapped.Item.NEW_TAB)
                )
                activity.browsingModeManager.mode = BrowsingMode.Normal
                navController.navigate(
                    BrowserFragmentDirections.actionGlobalHome(focusOnAddressBar = true)
                )
            }
            is TabCounterMenu.Item.NewPrivateTab -> {
                metrics.track(
                    Event.TabCounterMenuItemTapped(
                        Event.TabCounterMenuItemTapped.Item.NEW_PRIVATE_TAB
                    )
                )
                activity.browsingModeManager.mode = BrowsingMode.Private
                navController.navigate(
                    BrowserFragmentDirections.actionGlobalHome(focusOnAddressBar = true)
                )
            }
        }
    }

    override fun handleScroll(offset: Int) {
        if (activity.settings().isDynamicToolbarEnabled) {
            engineView.setVerticalClipping(offset)
        }
    }

    companion object {
        internal const val TELEMETRY_BROWSER_IDENTIFIER = "browserMenu"
    }
}

private fun BrowserStore.updateSearchTermsOfSelectedSession(
    searchTerms: String
) {
    val selectedTabId = state.selectedTabId ?: return

    dispatch(ContentAction.UpdateSearchTermsAction(
        selectedTabId,
        searchTerms
    ))
}
