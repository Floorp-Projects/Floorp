/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import android.content.Intent
import android.view.ViewGroup
import androidx.lifecycle.LifecycleCoroutineScope
import androidx.navigation.NavController
import androidx.navigation.NavDirections
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.verify
import io.mockk.verifyOrder
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.ObsoleteCoroutinesApi
import kotlinx.coroutines.newSingleThreadContext
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.setMain
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.EngineView
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.tabs.TabsUseCases
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserFragment
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.collections.SaveCollectionStep
import org.mozilla.fenix.components.Analytics
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.toTab
import org.mozilla.fenix.home.Tab
import org.mozilla.fenix.settings.deletebrowsingdata.deleteAndQuit

@ExperimentalCoroutinesApi
@UseExperimental(ObsoleteCoroutinesApi::class)
class DefaultBrowserToolbarControllerTest {

    private val mainThreadSurrogate = newSingleThreadContext("UI thread")
    private var browserLayout: ViewGroup = mockk(relaxed = true)
    private var swipeRefreshLayout: SwipeRefreshLayout = mockk(relaxed = true)
    private var activity: HomeActivity = mockk(relaxed = true)
    private var analytics: Analytics = mockk(relaxed = true)
    private val browsingModeManager: BrowsingModeManager = mockk(relaxed = true)
    private var navController: NavController = mockk(relaxed = true)
    private var findInPageLauncher: () -> Unit = mockk(relaxed = true)
    private val engineView: EngineView = mockk(relaxed = true)
    private val currentSession: Session = mockk(relaxed = true)
    private val getSupportUrl: () -> String = { "https://supportUrl.org" }
    private val openInFenixIntent: Intent = mockk(relaxed = true)
    private val currentSessionAsTab: Tab = mockk(relaxed = true)
    private val metrics: MetricController = mockk(relaxed = true)
    private val searchUseCases: SearchUseCases = mockk(relaxed = true)
    private val sessionUseCases: SessionUseCases = mockk(relaxed = true)
    private val scope: LifecycleCoroutineScope = mockk(relaxed = true)
    private val adjustBackgroundAndNavigate: (NavDirections) -> Unit = mockk(relaxed = true)
    private val snackbar = mockk<FenixSnackbar>(relaxed = true)
    private val tabCollectionStorage = mockk<TabCollectionStorage>(relaxed = true)

    private lateinit var controller: DefaultBrowserToolbarController

    @Before
    fun setUp() {
        Dispatchers.setMain(mainThreadSurrogate)

        controller = DefaultBrowserToolbarController(
            activity = activity,
            snackbar = snackbar,
            navController = navController,
            browsingModeManager = browsingModeManager,
            findInPageLauncher = findInPageLauncher,
            engineView = engineView,
            adjustBackgroundAndNavigate = adjustBackgroundAndNavigate,
            customTabSession = null,
            getSupportUrl = getSupportUrl,
            openInFenixIntent = openInFenixIntent,
            scope = scope,
            browserLayout = browserLayout,
            swipeRefresh = swipeRefreshLayout,
            tabCollectionStorage = tabCollectionStorage,
            bookmarkTapped = mockk(),
            readerModeController = mockk(),
            sessionManager = mockk(),
            store = mockk()
        )

        mockkStatic(
            "org.mozilla.fenix.ext.SessionKt"
        )
        every { any<Session>().toTab(any<Context>()) } returns currentSessionAsTab

        mockkStatic(
            "org.mozilla.fenix.settings.deletebrowsingdata.DeleteAndQuitKt"
        )
        every { deleteAndQuit(any(), any(), snackbar) } just Runs

        every { activity.components.analytics } returns analytics
        every { analytics.metrics } returns metrics
        every { activity.components.useCases.sessionUseCases } returns sessionUseCases
        every { activity.components.useCases.searchUseCases } returns searchUseCases
        every { activity.components.core.sessionManager.selectedSession } returns currentSession
        every { adjustBackgroundAndNavigate.invoke(any()) } just Runs
    }

    @Test
    fun handleBrowserToolbarPaste() {
        every { currentSession.id } returns "1"

        val pastedText = "Mozilla"
        controller.handleToolbarPaste(pastedText)

        verify {
            adjustBackgroundAndNavigate.invoke(
                BrowserFragmentDirections.actionBrowserFragmentToSearchFragment(
                    sessionId = "1",
                    pastedText = pastedText
                )
            )
        }
    }

    @Test
    fun handleBrowserToolbarPasteAndGoSearch() {
        val pastedText = "Mozilla"

        controller.handleToolbarPasteAndGo(pastedText)
        verifyOrder {
            currentSession.searchTerms = "Mozilla"
            searchUseCases.defaultSearch.invoke(pastedText)
        }
    }

    @Test
    fun handleBrowserToolbarPasteAndGoUrl() {
        val pastedText = "https://mozilla.org"

        controller.handleToolbarPasteAndGo(pastedText)
        verifyOrder {
            currentSession.searchTerms = ""
            sessionUseCases.loadUrl(pastedText)
        }
    }

    @After
    fun tearDown() {
        Dispatchers.resetMain() // reset main dispatcher to the original Main dispatcher
        mainThreadSurrogate.close()
    }

    @Test
    fun handleToolbarClick() {
        every { currentSession.id } returns "1"

        controller.handleToolbarClick()

        verify { metrics.track(Event.SearchBarTapped(Event.SearchBarTapped.Source.BROWSER)) }
        verify {
            adjustBackgroundAndNavigate.invoke(
                BrowserFragmentDirections.actionBrowserFragmentToSearchFragment(
                    sessionId = "1"
                )
            )
        }
    }

    @Test
    fun handleToolbarBackPress() {
        val item = ToolbarMenu.Item.Back

        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.BACK)) }
        verify { sessionUseCases.goBack }
    }

    @Test
    fun handleToolbarForwardPress() {
        val item = ToolbarMenu.Item.Forward

        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.FORWARD)) }
        verify { sessionUseCases.goForward }
    }

    @Test
    fun handleToolbarReloadPress() {
        val item = ToolbarMenu.Item.Reload

        every { activity.components.useCases.sessionUseCases } returns sessionUseCases

        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.RELOAD)) }
        verify { sessionUseCases.reload }
    }

    @Test
    fun handleToolbarStopPress() {
        val item = ToolbarMenu.Item.Stop

        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.STOP)) }
        verify { sessionUseCases.stopLoading }
    }

    @Test
    fun handleToolbarSettingsPress() {
        val item = ToolbarMenu.Item.Settings

        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.SETTINGS)) }
        verify {
            adjustBackgroundAndNavigate.invoke(
                BrowserFragmentDirections.actionBrowserFragmentToSettingsFragment()
            )
        }
    }

    @Test
    fun handleToolbarLibraryPress() {
        val item = ToolbarMenu.Item.Library

        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.LIBRARY)) }
        verify {
            adjustBackgroundAndNavigate.invoke(
                BrowserFragmentDirections.actionBrowserFragmentToLibraryFragment()
            )
        }
    }

    @Test
    fun handleToolbarRequestDesktopOnPress() {
        val requestDesktopSiteUseCase: SessionUseCases.RequestDesktopSiteUseCase =
            mockk(relaxed = true)
        val item = ToolbarMenu.Item.RequestDesktop(true)

        every { sessionUseCases.requestDesktopSite } returns requestDesktopSiteUseCase

        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.DESKTOP_VIEW_ON)) }
        verify {
            requestDesktopSiteUseCase.invoke(
                true,
                currentSession
            )
        }
    }

    @Test
    fun handleToolbarRequestDesktopOffPress() {
        val requestDesktopSiteUseCase: SessionUseCases.RequestDesktopSiteUseCase =
            mockk(relaxed = true)
        val item = ToolbarMenu.Item.RequestDesktop(false)

        every { sessionUseCases.requestDesktopSite } returns requestDesktopSiteUseCase

        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.DESKTOP_VIEW_OFF)) }
        verify {
            requestDesktopSiteUseCase.invoke(
                false,
                currentSession
            )
        }
    }

    @Test
    fun handleToolbarAddToHomeScreenPress() {
        val item = ToolbarMenu.Item.AddToHomeScreen

        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.ADD_TO_HOMESCREEN)) }
    }

    @Test
    fun handleToolbarSharePress() {
        val item = ToolbarMenu.Item.Share

        every { currentSession.url } returns "https://mozilla.org"

        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.SHARE)) }
        verify { navController.navigate(any<NavDirections>()) }
    }

    @Test
    fun handleToolbarNewPrivateTabPress() {
        val item = ToolbarMenu.Item.NewPrivateTab

        every { browsingModeManager.mode } returns BrowsingMode.Normal

        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.NEW_PRIVATE_TAB)) }
        verify {
            val directions = BrowserFragmentDirections
                .actionBrowserFragmentToSearchFragment(sessionId = null)
            adjustBackgroundAndNavigate.invoke(directions)
        }
        verify { browsingModeManager.mode = BrowsingMode.Private }
    }

    @Test
    fun handleToolbarFindInPagePress() {
        val item = ToolbarMenu.Item.FindInPage

        controller.handleToolbarItemInteraction(item)

        verify { findInPageLauncher() }
        verify { metrics.track(Event.FindInPageOpened) }
    }

    @Test
    fun handleToolbarReportIssuePress() {
        val tabsUseCases: TabsUseCases = mockk(relaxed = true)
        val addTabUseCase: TabsUseCases.AddNewTabUseCase = mockk(relaxed = true)

        val item = ToolbarMenu.Item.ReportIssue

        every { currentSession.id } returns "1"
        every { currentSession.url } returns "https://mozilla.org"
        every { activity.components.useCases.tabsUseCases } returns tabsUseCases
        every { tabsUseCases.addTab } returns addTabUseCase

        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.REPORT_SITE_ISSUE)) }
        verify {
            // Hardcoded URL because this function modifies the URL with an apply
            addTabUseCase.invoke(
                String.format(
                    BrowserFragment.REPORT_SITE_ISSUE_URL,
                    "https://mozilla.org"
                )
            )
        }
    }

    @Test
    fun handleToolbarHelpPress() {
        val tabsUseCases: TabsUseCases = mockk(relaxed = true)
        val addTabUseCase: TabsUseCases.AddNewTabUseCase = mockk(relaxed = true)

        val item = ToolbarMenu.Item.Help

        every { activity.components.useCases.tabsUseCases } returns tabsUseCases
        every { tabsUseCases.addTab } returns addTabUseCase

        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.HELP)) }
        verify {
            addTabUseCase.invoke(getSupportUrl())
        }
    }

    @Test
    fun handleToolbarNewTabPress() {
        val item = ToolbarMenu.Item.NewTab

        every { browsingModeManager.mode } returns BrowsingMode.Private

        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.NEW_TAB)) }
        verify {
            val directions = BrowserFragmentDirections
                .actionBrowserFragmentToSearchFragment(sessionId = null)
            adjustBackgroundAndNavigate.invoke(directions)
        }
        verify { browsingModeManager.mode = BrowsingMode.Normal }
    }

    @Test
    fun handleToolbarSaveToCollectionPressWhenAtLeastOneCollectionExists() {
        val item = ToolbarMenu.Item.SaveToCollection
        val cachedTabCollections: List<TabCollection> = mockk(relaxed = true)
        every { activity.components.useCases.sessionUseCases } returns sessionUseCases
        every { activity.components.core.tabCollectionStorage.cachedTabCollections } returns cachedTabCollections

        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(
            Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.SAVE_TO_COLLECTION)) }
        verify { metrics.track(
            Event.CollectionSaveButtonPressed(DefaultBrowserToolbarController.TELEMETRY_BROWSER_IDENTIFIER)) }
        verify {
            val directions =
                BrowserFragmentDirections.actionBrowserFragmentToCreateCollectionFragment(
                    previousFragmentId = R.id.browserFragment,
                    saveCollectionStep = SaveCollectionStep.SelectCollection,
                    tabIds = arrayOf(currentSession.id),
                    selectedTabIds = arrayOf(currentSession.id)
                )
            navController.nav(R.id.browserFragment, directions)
        }
    }

    @Test
    fun handleToolbarSaveToCollectionPressWhenNoCollectionsExists() {
        val item = ToolbarMenu.Item.SaveToCollection
        val cachedTabCollectionsEmpty: List<TabCollection> = emptyList()
        every { activity.components.useCases.sessionUseCases } returns sessionUseCases
        every { activity.components.core.tabCollectionStorage.cachedTabCollections } returns cachedTabCollectionsEmpty

        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.SAVE_TO_COLLECTION)) }
        verify { metrics.track(Event.CollectionSaveButtonPressed(
            DefaultBrowserToolbarController.TELEMETRY_BROWSER_IDENTIFIER)) }
        verify {
            val directions =
                BrowserFragmentDirections.actionBrowserFragmentToCreateCollectionFragment(
                    previousFragmentId = R.id.browserFragment,
                    saveCollectionStep = SaveCollectionStep.NameCollection,
                    tabIds = arrayOf(currentSession.id),
                    selectedTabIds = arrayOf(currentSession.id)
                )
            navController.nav(R.id.browserFragment, directions)
        }
    }

    @Test
    fun handleToolbarOpenInFenixPress() {
        controller = DefaultBrowserToolbarController(
            activity = activity,
            snackbar = snackbar,
            navController = navController,
            browsingModeManager = browsingModeManager,
            findInPageLauncher = findInPageLauncher,
            engineView = engineView,
            adjustBackgroundAndNavigate = adjustBackgroundAndNavigate,
            customTabSession = currentSession,
            getSupportUrl = getSupportUrl,
            openInFenixIntent = openInFenixIntent,
            scope = scope,
            browserLayout = browserLayout,
            swipeRefresh = swipeRefreshLayout,
            tabCollectionStorage = tabCollectionStorage,
            bookmarkTapped = mockk(),
            readerModeController = mockk(),
            sessionManager = mockk(),
            store = mockk()
        )

        val sessionManager: SessionManager = mockk(relaxed = true)
        val item = ToolbarMenu.Item.OpenInFenix

        every { activity.components.core.sessionManager } returns sessionManager
        every { currentSession.customTabConfig } returns mockk()
        every { activity.startActivity(any()) } just Runs

        controller.handleToolbarItemInteraction(item)

        verify { engineView.release() }
        verify { currentSession.customTabConfig = null }
        verify { sessionManager.select(currentSession) }
        verify { activity.startActivity(openInFenixIntent) }
        verify { activity.finish() }
    }

    @Test
    fun handleToolbarQuitPress() {
        val item = ToolbarMenu.Item.Quit

        controller.handleToolbarItemInteraction(item)

        verify { deleteAndQuit(activity, scope, snackbar) }
    }
}
