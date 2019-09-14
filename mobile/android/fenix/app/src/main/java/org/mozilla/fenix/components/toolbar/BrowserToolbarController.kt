/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.app.Activity
import android.content.Intent
import androidx.core.widget.NestedScrollView
import androidx.lifecycle.LifecycleCoroutineScope
import androidx.navigation.NavController
import com.google.android.material.bottomsheet.BottomSheetBehavior
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.ObsoleteCoroutinesApi
import kotlinx.coroutines.launch
import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.EngineView
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserFragment
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.collections.CreateCollectionViewModel
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.toTab
import org.mozilla.fenix.lib.Do
import org.mozilla.fenix.quickactionsheet.QuickActionSheetBehavior
import org.mozilla.fenix.utils.deleteAndQuit

/**
 * An interface that handles the view manipulation of the BrowserToolbar, triggered by the Interactor
 */
interface BrowserToolbarController {
    fun handleToolbarPaste(text: String)
    fun handleToolbarPasteAndGo(text: String)
    fun handleToolbarItemInteraction(item: ToolbarMenu.Item)
    fun handleToolbarClick()
}

class DefaultBrowserToolbarController(
    private val activity: Activity,
    private val navController: NavController,
    private val browsingModeManager: BrowsingModeManager,
    private val findInPageLauncher: () -> Unit,
    private val engineView: EngineView,
    private val customTabSession: Session?,
    private val viewModel: CreateCollectionViewModel,
    private val getSupportUrl: () -> String,
    private val openInFenixIntent: Intent,
    private val bottomSheetBehavior: QuickActionSheetBehavior<NestedScrollView>,
    private val scope: LifecycleCoroutineScope
) : BrowserToolbarController {

    private val currentSession
        get() = customTabSession ?: activity.components.core.sessionManager.selectedSession

    override fun handleToolbarPaste(text: String) {
        navController.nav(
            R.id.browserFragment,
            BrowserFragmentDirections.actionBrowserFragmentToSearchFragment(
                sessionId = currentSession?.id,
                pastedText = text
            )
        )
    }

    override fun handleToolbarPasteAndGo(text: String) {
        activity.components.core.sessionManager.selectedSession?.searchTerms = ""
        activity.components.useCases.sessionUseCases.loadUrl(text)
    }

    override fun handleToolbarClick() {
        activity.components.analytics.metrics.track(
            Event.SearchBarTapped(Event.SearchBarTapped.Source.BROWSER)
        )
        navController.nav(
            R.id.browserFragment,
            BrowserFragmentDirections.actionBrowserFragmentToSearchFragment(currentSession?.id)
        )
    }

    @ExperimentalCoroutinesApi
    @ObsoleteCoroutinesApi
    @SuppressWarnings("ComplexMethod", "LongMethod")
    override fun handleToolbarItemInteraction(item: ToolbarMenu.Item) {
        val sessionUseCases = activity.components.useCases.sessionUseCases
        trackToolbarItemInteraction(item)

        Do exhaustive when (item) {
            ToolbarMenu.Item.Back -> sessionUseCases.goBack.invoke(currentSession)
            ToolbarMenu.Item.Forward -> sessionUseCases.goForward.invoke(currentSession)
            ToolbarMenu.Item.Reload -> sessionUseCases.reload.invoke(currentSession)
            ToolbarMenu.Item.Stop -> sessionUseCases.stopLoading.invoke(currentSession)
            ToolbarMenu.Item.Settings -> navController.nav(
                R.id.browserFragment,
                BrowserFragmentDirections.actionBrowserFragmentToSettingsFragment()
            )
            ToolbarMenu.Item.Library -> navController.nav(
                R.id.browserFragment,
                BrowserFragmentDirections.actionBrowserFragmentToLibraryFragment()
            )
            is ToolbarMenu.Item.RequestDesktop -> sessionUseCases.requestDesktopSite.invoke(
                item.isChecked,
                currentSession
            )
            ToolbarMenu.Item.AddToHomeScreen -> {
                MainScope().launch {
                    with(activity.components.useCases.webAppUseCases) {
                        if (isInstallable()) {
                            addToHomescreen()
                        } else {
                            val directions = BrowserFragmentDirections.actionBrowserFragmentToCreateShortcutFragment()
                            navController.navigate(directions)
                        }
                    }
                }
            }
            ToolbarMenu.Item.Share -> {
                val currentUrl = currentSession?.url
                currentUrl?.apply {
                    val directions = NavGraphDirections.actionGlobalShareFragment(this)
                    navController.navigate(directions)
                }
            }
            ToolbarMenu.Item.NewTab -> {
                val directions = BrowserFragmentDirections.actionBrowserFragmentToSearchFragment(
                    sessionId = null
                )
                navController.nav(R.id.browserFragment, directions)
                browsingModeManager.mode = BrowsingMode.Normal
            }
            ToolbarMenu.Item.NewPrivateTab -> {
                val directions = BrowserFragmentDirections.actionBrowserFragmentToSearchFragment(
                    sessionId = null
                )
                navController.nav(R.id.browserFragment, directions)
                browsingModeManager.mode = BrowsingMode.Private
            }
            ToolbarMenu.Item.FindInPage -> {
                bottomSheetBehavior.state = BottomSheetBehavior.STATE_COLLAPSED
                findInPageLauncher()
                activity.components.analytics.metrics.track(Event.FindInPageOpened)
            }
            ToolbarMenu.Item.ReportIssue -> {
                val currentUrl = currentSession?.url
                currentUrl?.apply {
                    val reportUrl = String.format(BrowserFragment.REPORT_SITE_ISSUE_URL, this)
                    activity.components.useCases.tabsUseCases.addTab.invoke(reportUrl)
                }
            }
            ToolbarMenu.Item.Help -> {
                activity.components.useCases.tabsUseCases.addTab.invoke(getSupportUrl())
            }
            ToolbarMenu.Item.SaveToCollection -> {
                activity.components.analytics.metrics
                    .track(Event.CollectionSaveButtonPressed(TELEMETRY_BROWSER_IDENTIFIER))

                currentSession?.toTab(activity)?.let { currentSessionAsTab ->
                    viewModel.saveTabToCollection(
                        tabs = listOf(currentSessionAsTab),
                        selectedTab = currentSessionAsTab,
                        cachedTabCollections = activity.components.core.tabCollectionStorage.cachedTabCollections
                    )
                    viewModel.previousFragmentId = R.id.browserFragment

                    val directions = BrowserFragmentDirections.actionBrowserFragmentToCreateCollectionFragment()
                    navController.nav(R.id.browserFragment, directions)
                }
            }
            ToolbarMenu.Item.OpenInFenix -> {
                // Release the session from this view so that it can immediately be rendered by a different view
                engineView.release()

                // Strip the CustomTabConfig to turn this Session into a regular tab and then select it
                customTabSession!!.customTabConfig = null
                activity.components.core.sessionManager.select(customTabSession)

                // Switch to the actual browser which should now display our new selected session
                activity.startActivity(openInFenixIntent)

                // Close this activity since it is no longer displaying any session
                activity.finish()
            }
            ToolbarMenu.Item.Quit -> deleteAndQuit(activity, scope)
        }
    }

    @SuppressWarnings("ComplexMethod")
    private fun trackToolbarItemInteraction(item: ToolbarMenu.Item) {
        val eventItem = when (item) {
            ToolbarMenu.Item.Back -> Event.BrowserMenuItemTapped.Item.BACK
            ToolbarMenu.Item.Forward -> Event.BrowserMenuItemTapped.Item.FORWARD
            ToolbarMenu.Item.Reload -> Event.BrowserMenuItemTapped.Item.RELOAD
            ToolbarMenu.Item.Stop -> Event.BrowserMenuItemTapped.Item.STOP
            ToolbarMenu.Item.Settings -> Event.BrowserMenuItemTapped.Item.SETTINGS
            ToolbarMenu.Item.Library -> Event.BrowserMenuItemTapped.Item.LIBRARY
            is ToolbarMenu.Item.RequestDesktop ->
                if (item.isChecked) {
                    Event.BrowserMenuItemTapped.Item.DESKTOP_VIEW_ON
                } else {
                    Event.BrowserMenuItemTapped.Item.DESKTOP_VIEW_OFF
                }

            ToolbarMenu.Item.NewPrivateTab -> Event.BrowserMenuItemTapped.Item.NEW_PRIVATE_TAB
            ToolbarMenu.Item.FindInPage -> Event.BrowserMenuItemTapped.Item.FIND_IN_PAGE
            ToolbarMenu.Item.ReportIssue -> Event.BrowserMenuItemTapped.Item.REPORT_SITE_ISSUE
            ToolbarMenu.Item.Help -> Event.BrowserMenuItemTapped.Item.HELP
            ToolbarMenu.Item.NewTab -> Event.BrowserMenuItemTapped.Item.NEW_TAB
            ToolbarMenu.Item.OpenInFenix -> Event.BrowserMenuItemTapped.Item.OPEN_IN_FENIX
            ToolbarMenu.Item.Share -> Event.BrowserMenuItemTapped.Item.SHARE
            ToolbarMenu.Item.SaveToCollection -> Event.BrowserMenuItemTapped.Item.SAVE_TO_COLLECTION
            ToolbarMenu.Item.AddToHomeScreen -> Event.BrowserMenuItemTapped.Item.ADD_TO_HOMESCREEN
            ToolbarMenu.Item.Quit -> Event.BrowserMenuItemTapped.Item.QUIT
        }

        activity.components.analytics.metrics.track(Event.BrowserMenuItemTapped(eventItem))
    }

    companion object {
        internal const val TELEMETRY_BROWSER_IDENTIFIER = "browserMenu"
    }
}
