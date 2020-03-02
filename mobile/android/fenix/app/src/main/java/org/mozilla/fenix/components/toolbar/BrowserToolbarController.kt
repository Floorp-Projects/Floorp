/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.app.Activity
import android.content.Intent
import androidx.annotation.VisibleForTesting
import androidx.navigation.NavController
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout
import com.google.android.material.snackbar.Snackbar
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.support.ktx.kotlin.isUrl
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserAnimator
import org.mozilla.fenix.browser.BrowserFragment
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.browser.readermode.ReaderModeController
import org.mozilla.fenix.collections.SaveCollectionStep
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.components.TopSiteStorage
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getRootView
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.lib.Do
import org.mozilla.fenix.settings.deletebrowsingdata.deleteAndQuit

/**
 * An interface that handles the view manipulation of the BrowserToolbar, triggered by the Interactor
 */
interface BrowserToolbarController {
    fun handleToolbarPaste(text: String)
    fun handleToolbarPasteAndGo(text: String)
    fun handleToolbarItemInteraction(item: ToolbarMenu.Item)
    fun handleToolbarClick()
    fun handleTabCounterClick()
    fun handleBrowserMenuDismissed(lowPrioHighlightItems: List<ToolbarMenu.Item>)
}

@Suppress("LargeClass")
class DefaultBrowserToolbarController(
    private val store: BrowserFragmentStore,
    private val activity: Activity,
    private val navController: NavController,
    private val readerModeController: ReaderModeController,
    private val browsingModeManager: BrowsingModeManager,
    private val sessionManager: SessionManager,
    private val findInPageLauncher: () -> Unit,
    private val engineView: EngineView,
    private val browserAnimator: BrowserAnimator,
    private val swipeRefresh: SwipeRefreshLayout,
    private val customTabSession: Session?,
    private val getSupportUrl: () -> String,
    private val openInFenixIntent: Intent,
    private val bookmarkTapped: (Session) -> Unit,
    private val scope: CoroutineScope,
    private val tabCollectionStorage: TabCollectionStorage,
    private val topSiteStorage: TopSiteStorage
) : BrowserToolbarController {

    private val currentSession
        get() = customTabSession ?: activity.components.core.sessionManager.selectedSession

    // We hold onto a reference of the inner scope so that we can override this with the
    // TestCoroutineScope to ensure sequential execution. If we didn't have this, our tests
    // would fail intermittently due to the async nature of coroutine scheduling.
    @VisibleForTesting
    internal var ioScope: CoroutineScope = CoroutineScope(Dispatchers.IO)

    override fun handleToolbarPaste(text: String) {
        browserAnimator.captureEngineViewAndDrawStatically {
            val directions = BrowserFragmentDirections.actionBrowserFragmentToSearchFragment(
                sessionId = currentSession?.id,
                pastedText = text
            )

            navController.nav(R.id.browserFragment, directions)
        }
    }

    override fun handleToolbarPasteAndGo(text: String) {
        if (text.isUrl()) {
            activity.components.core.sessionManager.selectedSession?.searchTerms = ""
            activity.components.useCases.sessionUseCases.loadUrl.invoke(text)
            return
        }

        activity.components.core.sessionManager.selectedSession?.searchTerms = text
        activity.components.useCases.searchUseCases.defaultSearch.invoke(text)
    }

    override fun handleToolbarClick() {
        activity.components.analytics.metrics.track(
            Event.SearchBarTapped(Event.SearchBarTapped.Source.BROWSER)
        )

        browserAnimator.captureEngineViewAndDrawStatically {
            val directions = BrowserFragmentDirections.actionBrowserFragmentToSearchFragment(
                currentSession?.id
            )

            navController.nav(R.id.browserFragment, directions)
        }
    }

    override fun handleTabCounterClick() {
        animateTabAndNavigateHome()
    }

    override fun handleBrowserMenuDismissed(lowPrioHighlightItems: List<ToolbarMenu.Item>) {
        lowPrioHighlightItems.forEach {
            when (it) {
                ToolbarMenu.Item.AddToHomeScreen -> activity.settings().installPwaOpened = true
                is ToolbarMenu.Item.ReaderMode -> activity.settings().readerModeOpened = true
                ToolbarMenu.Item.OpenInApp -> activity.settings().openInAppOpened = true
                else -> {}
            }
        }
    }

    @ExperimentalCoroutinesApi
    @SuppressWarnings("ComplexMethod", "LongMethod")
    override fun handleToolbarItemInteraction(item: ToolbarMenu.Item) {
        val sessionUseCases = activity.components.useCases.sessionUseCases
        trackToolbarItemInteraction(item)

        Do exhaustive when (item) {
            ToolbarMenu.Item.Back -> sessionUseCases.goBack.invoke(currentSession)
            ToolbarMenu.Item.Forward -> sessionUseCases.goForward.invoke(currentSession)
            ToolbarMenu.Item.Reload -> sessionUseCases.reload.invoke(currentSession)
            ToolbarMenu.Item.Stop -> sessionUseCases.stopLoading.invoke(currentSession)
            ToolbarMenu.Item.Settings -> browserAnimator.captureEngineViewAndDrawStatically {
                val directions = BrowserFragmentDirections.actionBrowserFragmentToSettingsFragment()
                navController.nav(R.id.browserFragment, directions)
            }
            ToolbarMenu.Item.Library -> browserAnimator.captureEngineViewAndDrawStatically {
                val directions = BrowserFragmentDirections.actionBrowserFragmentToLibraryFragment()
                navController.nav(R.id.browserFragment, directions)
            }
            is ToolbarMenu.Item.RequestDesktop -> sessionUseCases.requestDesktopSite.invoke(
                item.isChecked,
                currentSession
            )
            ToolbarMenu.Item.AddToTopSites -> {
                ioScope.launch {
                    currentSession?.let {
                        topSiteStorage.addTopSite(it.title, it.url)
                    }
                    MainScope().launch {
                        FenixSnackbar.make(swipeRefresh, Snackbar.LENGTH_SHORT)
                            .setText(
                                swipeRefresh.context.getString(R.string.snackbar_added_to_top_sites)
                            )
                            .show()
                    }
                }
            }
            ToolbarMenu.Item.AddToHomeScreen -> {
                activity.settings().installPwaOpened = true
                MainScope().launch {
                    with(activity.components.useCases.webAppUseCases) {
                        if (isInstallable()) {
                            addToHomescreen()
                        } else {
                            val directions =
                                BrowserFragmentDirections.actionBrowserFragmentToCreateShortcutFragment()
                            navController.navigate(directions)
                        }
                    }
                }
            }
            ToolbarMenu.Item.Share -> {
                val directions = NavGraphDirections.actionGlobalShareFragment(
                    data = arrayOf(
                        ShareData(
                            url = currentSession?.url,
                            title = currentSession?.title
                        )
                    ),
                    showPage = true
                )
                navController.navigate(directions)
            }

            ToolbarMenu.Item.FindInPage -> {
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

            ToolbarMenu.Item.AddonsManager -> {
                navController.nav(
                    R.id.browserFragment,
                    BrowserFragmentDirections
                        .actionBrowserFragmentToAddonsManagementFragment()
                )
            }
            ToolbarMenu.Item.SaveToCollection -> {
                activity.components.analytics.metrics
                    .track(Event.CollectionSaveButtonPressed(TELEMETRY_BROWSER_IDENTIFIER))

                currentSession?.let { currentSession ->
                    val directions =
                        BrowserFragmentDirections.actionBrowserFragmentToCreateCollectionFragment(
                            previousFragmentId = R.id.browserFragment,
                            tabIds = arrayOf(currentSession.id),
                            selectedTabIds = arrayOf(currentSession.id),
                            saveCollectionStep = if (tabCollectionStorage.cachedTabCollections.isEmpty()) {
                                SaveCollectionStep.NameCollection
                            } else {
                                SaveCollectionStep.SelectCollection
                            }
                        )
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
            ToolbarMenu.Item.Quit -> {
                // We need to show the snackbar while the browsing data is deleting (if "Delete
                // browsing data on quit" is activated). After the deletion is over, the snackbar
                // is dismissed.
                val snackbar: FenixSnackbar? = activity.getRootView()?.let { v ->
                    FenixSnackbar.makeWithToolbarPadding(v)
                        .setText(v.context.getString(R.string.deleting_browsing_data_in_progress))
                }

                deleteAndQuit(activity, scope, snackbar)
            }
            is ToolbarMenu.Item.ReaderMode -> {
                activity.settings().readerModeOpened = true

                val enabled = currentSession?.readerMode
                    ?: activity.components.core.sessionManager.selectedSession?.readerMode
                    ?: false

                if (enabled) {
                    readerModeController.hideReaderView()
                } else {
                    readerModeController.showReaderView()
                }
            }
            ToolbarMenu.Item.ReaderModeAppearance -> {
                readerModeController.showControls()
            }
            ToolbarMenu.Item.OpenInApp -> {
                activity.settings().openInAppOpened = true

                val appLinksUseCases =
                    activity.components.useCases.appLinksUseCases
                val getRedirect = appLinksUseCases.appLinkRedirect
                sessionManager.selectedSession?.let {
                    val redirect = getRedirect.invoke(it.url)
                    redirect.appIntent?.flags = Intent.FLAG_ACTIVITY_NEW_TASK
                    appLinksUseCases.openAppLink.invoke(redirect.appIntent)
                }
            }
            ToolbarMenu.Item.Bookmark -> {
                sessionManager.selectedSession?.let {
                    bookmarkTapped(it)
                }
            }
        }
    }

    private fun animateTabAndNavigateHome() {
        scope.launch {
            browserAnimator.beginAnimateOut()
            // Delay for a short amount of time so the browser has time to start animating out
            // before we transition the fragment. This makes the animation feel smoother
            delay(ANIMATION_DELAY)
            if (!navController.popBackStack(R.id.homeFragment, false)) {
                val directions = BrowserFragmentDirections.actionBrowserFragmentToHomeFragment()
                navController.nav(
                    R.id.browserFragment,
                    directions,
                    null
                )
            }
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

            ToolbarMenu.Item.FindInPage -> Event.BrowserMenuItemTapped.Item.FIND_IN_PAGE
            ToolbarMenu.Item.ReportIssue -> Event.BrowserMenuItemTapped.Item.REPORT_SITE_ISSUE
            ToolbarMenu.Item.OpenInFenix -> Event.BrowserMenuItemTapped.Item.OPEN_IN_FENIX
            ToolbarMenu.Item.Share -> Event.BrowserMenuItemTapped.Item.SHARE
            ToolbarMenu.Item.SaveToCollection -> Event.BrowserMenuItemTapped.Item.SAVE_TO_COLLECTION
            ToolbarMenu.Item.AddToTopSites -> Event.BrowserMenuItemTapped.Item.ADD_TO_TOP_SITES
            ToolbarMenu.Item.AddToHomeScreen -> Event.BrowserMenuItemTapped.Item.ADD_TO_HOMESCREEN
            ToolbarMenu.Item.Quit -> Event.BrowserMenuItemTapped.Item.QUIT
            is ToolbarMenu.Item.ReaderMode ->
                if (item.isChecked) {
                    Event.BrowserMenuItemTapped.Item.READER_MODE_ON
                } else {
                    Event.BrowserMenuItemTapped.Item.READER_MODE_OFF
                }
            ToolbarMenu.Item.ReaderModeAppearance ->
                Event.BrowserMenuItemTapped.Item.READER_MODE_APPEARANCE
            ToolbarMenu.Item.OpenInApp -> Event.BrowserMenuItemTapped.Item.OPEN_IN_APP
            ToolbarMenu.Item.Bookmark -> Event.BrowserMenuItemTapped.Item.BOOKMARK
            ToolbarMenu.Item.AddonsManager -> Event.BrowserMenuItemTapped.Item.ADDONS_MANAGER
        }

        activity.components.analytics.metrics.track(Event.BrowserMenuItemTapped(eventItem))
    }

    companion object {
        const val ANIMATION_DELAY = 50L
        internal const val TELEMETRY_BROWSER_IDENTIFIER = "browserMenu"
    }
}
