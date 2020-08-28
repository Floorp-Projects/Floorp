/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import androidx.navigation.NavController
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.EngineView
import mozilla.components.support.ktx.kotlin.isUrl
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserAnimator
import org.mozilla.fenix.browser.BrowserAnimator.Companion.getToolbarNavOptions
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.readermode.ReaderModeController
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.sessionsOfType

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
    private val activity: HomeActivity,
    private val navController: NavController,
    private val metrics: MetricController,
    private val readerModeController: ReaderModeController,
    private val sessionManager: SessionManager,
    private val engineView: EngineView,
    private val browserAnimator: BrowserAnimator,
    private val customTabSession: Session?,
    private val useNewSearchExperience: Boolean = FeatureFlags.newSearchExperience,
    private val onTabCounterClicked: () -> Unit,
    private val onCloseTab: (Session) -> Unit
) : BrowserToolbarController {

    private val currentSession
        get() = customTabSession ?: sessionManager.selectedSession

    override fun handleToolbarPaste(text: String) {
        if (useNewSearchExperience) {
            navController.nav(
                R.id.browserFragment,
                BrowserFragmentDirections.actionGlobalSearchDialog(
                    sessionId = currentSession?.id,
                    pastedText = text
                ),
                getToolbarNavOptions(activity)
            )
        } else {
            browserAnimator.captureEngineViewAndDrawStatically {
                navController.nav(
                    R.id.browserFragment,
                    BrowserFragmentDirections.actionBrowserFragmentToSearchFragment(
                        sessionId = currentSession?.id,
                        pastedText = text
                    ),
                    getToolbarNavOptions(activity)
                )
            }
        }
    }

    override fun handleToolbarPasteAndGo(text: String) {
        if (text.isUrl()) {
            sessionManager.selectedSession?.searchTerms = ""
            activity.components.useCases.sessionUseCases.loadUrl.invoke(text)
            return
        }

        sessionManager.selectedSession?.searchTerms = text
        activity.components.useCases.searchUseCases.defaultSearch.invoke(
            text,
            session = sessionManager.selectedSession
        )
    }

    override fun handleToolbarClick() {
        metrics.track(Event.SearchBarTapped(Event.SearchBarTapped.Source.BROWSER))

        if (useNewSearchExperience) {
            navController.nav(
                R.id.browserFragment,
                BrowserFragmentDirections.actionGlobalSearchDialog(
                    currentSession?.id
                ),
                getToolbarNavOptions(activity)
            )
        } else {
            browserAnimator.captureEngineViewAndDrawStatically {
                navController.nav(
                    R.id.browserFragment,
                    BrowserFragmentDirections.actionBrowserFragmentToSearchFragment(
                        currentSession?.id
                    ),
                    getToolbarNavOptions(activity)
                )
            }
        }
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
                sessionManager.selectedSession?.let {
                    // When closing the last tab we must show the undo snackbar in the home fragment
                    if (sessionManager.sessionsOfType(it.private).count() == 1) {
                        // The tab tray always returns to normal mode so do that here too
                        activity.browsingModeManager.mode = BrowsingMode.Normal
                        navController.navigate(
                            BrowserFragmentDirections.actionGlobalHome(
                                sessionToDelete = it.id
                            )
                        )
                    } else {
                        onCloseTab.invoke(it)
                        activity.components.useCases.tabsUseCases.removeTab.invoke(it)
                    }
                }
            }
            is TabCounterMenu.Item.NewTab -> {
                activity.browsingModeManager.mode = item.mode
                navController.popBackStack(R.id.homeFragment, false)
            }
        }
    }

    override fun handleScroll(offset: Int) {
        engineView.setVerticalClipping(offset)
    }

    companion object {
        internal const val TELEMETRY_BROWSER_IDENTIFIER = "browserMenu"
    }
}
