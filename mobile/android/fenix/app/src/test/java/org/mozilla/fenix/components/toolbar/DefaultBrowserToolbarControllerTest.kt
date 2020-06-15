/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Intent
import androidx.navigation.NavController
import androidx.navigation.NavDirections
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout
import io.mockk.MockKAnnotations
import io.mockk.Runs
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.impl.annotations.RelaxedMockK
import io.mockk.just
import io.mockk.mockk
import io.mockk.mockkObject
import io.mockk.mockkStatic
import io.mockk.slot
import io.mockk.unmockkStatic
import io.mockk.verify
import io.mockk.verifyOrder
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runBlockingTest
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ReaderState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineView
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserAnimator
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.browser.browsingmode.DefaultBrowsingModeManager
import org.mozilla.fenix.browser.readermode.ReaderModeController
import org.mozilla.fenix.collections.SaveCollectionStep
import org.mozilla.fenix.components.Analytics
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.components.TopSiteStorage
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.toTab
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.Tab
import org.mozilla.fenix.settings.deletebrowsingdata.deleteAndQuit

@ExperimentalCoroutinesApi
@RunWith(FenixRobolectricTestRunner::class)
class DefaultBrowserToolbarControllerTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @MockK private lateinit var swipeRefreshLayout: SwipeRefreshLayout
    @RelaxedMockK private lateinit var activity: HomeActivity
    @RelaxedMockK private lateinit var analytics: Analytics
    @RelaxedMockK private lateinit var navController: NavController
    @RelaxedMockK private lateinit var findInPageLauncher: () -> Unit
    @RelaxedMockK private lateinit var bookmarkTapped: (Session) -> Unit
    @RelaxedMockK private lateinit var onTabCounterClicked: () -> Unit
    @RelaxedMockK private lateinit var onCloseTab: (Session) -> Unit
    @RelaxedMockK private lateinit var sessionManager: SessionManager
    @RelaxedMockK private lateinit var engineView: EngineView
    @RelaxedMockK private lateinit var currentSession: Session
    @RelaxedMockK private lateinit var openInFenixIntent: Intent
    @RelaxedMockK private lateinit var currentSessionAsTab: Tab
    @RelaxedMockK private lateinit var metrics: MetricController
    @RelaxedMockK private lateinit var searchUseCases: SearchUseCases
    @RelaxedMockK private lateinit var sessionUseCases: SessionUseCases
    @RelaxedMockK private lateinit var browserAnimator: BrowserAnimator
    @RelaxedMockK private lateinit var snackbar: FenixSnackbar
    @RelaxedMockK private lateinit var tabCollectionStorage: TabCollectionStorage
    @RelaxedMockK private lateinit var topSiteStorage: TopSiteStorage
    @RelaxedMockK private lateinit var readerModeController: ReaderModeController
    private val store: BrowserStore = BrowserStore(initialState = BrowserState(
        listOf(
            createTab("https://www.mozilla.org", id = "reader-inactive-tab"),
            createTab("https://www.mozilla.org", id = "reader-active-tab", readerState = ReaderState(active = true))
        ))
    )

    @Before
    fun setUp() {
        MockKAnnotations.init(this)

        mockkStatic(
            "org.mozilla.fenix.ext.SessionKt"
        )
        every { any<Session>().toTab(any()) } returns currentSessionAsTab

        mockkStatic(
            "org.mozilla.fenix.settings.deletebrowsingdata.DeleteAndQuitKt"
        )
        every { deleteAndQuit(any(), any(), any()) } just Runs

        mockkObject(FenixSnackbar.Companion)
        every { FenixSnackbar.make(any(), any(), any(), any()) } returns snackbar

        every { activity.components.analytics } returns analytics
        every { analytics.metrics } returns metrics
        every { activity.components.useCases.sessionUseCases } returns sessionUseCases
        every { activity.components.useCases.searchUseCases } returns searchUseCases
        every { activity.components.core.sessionManager } returns sessionManager
        every { activity.components.core.store } returns store
        every { sessionManager.selectedSession } returns currentSession

        val onComplete = slot<() -> Unit>()
        every { browserAnimator.captureEngineViewAndDrawStatically(capture(onComplete)) } answers { onComplete.captured.invoke() }
    }

    @After
    fun tearDown() {
        unmockkStatic("org.mozilla.fenix.ext.SessionKt")
        unmockkStatic("org.mozilla.fenix.settings.deletebrowsingdata.DeleteAndQuitKt")
    }

    @Test
    fun handleBrowserToolbarPaste() = runBlockingTest {
        every { currentSession.id } returns "1"

        val pastedText = "Mozilla"
        val controller = createController(scope = this)
        controller.handleToolbarPaste(pastedText)

        verify {
            val directions = BrowserFragmentDirections.actionBrowserFragmentToSearchFragment(
                sessionId = "1",
                pastedText = pastedText
            )
            navController.nav(R.id.browserFragment, directions)
        }
    }

    @Test
    fun handleBrowserToolbarPasteAndGoSearch() = runBlockingTest {
        val pastedText = "Mozilla"

        val controller = createController(scope = this)
        controller.handleToolbarPasteAndGo(pastedText)
        verifyOrder {
            currentSession.searchTerms = "Mozilla"
            searchUseCases.defaultSearch.invoke(pastedText)
        }
    }

    @Test
    fun handleBrowserToolbarPasteAndGoUrl() = runBlockingTest {
        val pastedText = "https://mozilla.org"

        val controller = createController(scope = this)
        controller.handleToolbarPasteAndGo(pastedText)
        verifyOrder {
            currentSession.searchTerms = ""
            sessionUseCases.loadUrl(pastedText)
        }
    }

    @Test
    fun handleTabCounterClick() = runBlockingTest {
        val controller = createController(scope = this)
        controller.handleTabCounterClick()

        verify { onTabCounterClicked() }
    }

    @Test
    fun `handle BrowserMenu dismissed with all options available`() = runBlockingTest {
        val itemList: List<ToolbarMenu.Item> = listOf(
            ToolbarMenu.Item.AddToHomeScreen,
            ToolbarMenu.Item.OpenInApp
        )

        val activity = HomeActivity()

        val controller = createController(scope = this, activity = activity)
        controller.handleBrowserMenuDismissed(itemList)

        assertEquals(true, activity.settings().installPwaOpened)
        assertEquals(true, activity.settings().openInAppOpened)
    }

    @Test
    fun handleToolbarClick() = runBlockingTest {
        every { currentSession.id } returns "1"

        val controller = createController(scope = this)
        controller.handleToolbarClick()

        verify { metrics.track(Event.SearchBarTapped(Event.SearchBarTapped.Source.BROWSER)) }
        verify {
            val directions = BrowserFragmentDirections.actionBrowserFragmentToSearchFragment(
                sessionId = "1"
            )
            navController.nav(R.id.browserFragment, directions)
        }
    }

    @Test
    fun handleToolbarBackPress() = runBlockingTest {
        val item = ToolbarMenu.Item.Back

        val controller = createController(scope = this)
        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.BACK)) }
        verify { sessionUseCases.goBack(currentSession) }
    }

    @Test
    fun handleToolbarForwardPress() = runBlockingTest {
        val item = ToolbarMenu.Item.Forward

        val controller = createController(scope = this)
        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.FORWARD)) }
        verify { sessionUseCases.goForward(currentSession) }
    }

    @Test
    fun handleToolbarReloadPress() = runBlockingTest {
        val item = ToolbarMenu.Item.Reload

        every { activity.components.useCases.sessionUseCases } returns sessionUseCases

        val controller = createController(scope = this)
        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.RELOAD)) }
        verify { sessionUseCases.reload(currentSession) }
    }

    @Test
    fun handleToolbarStopPress() = runBlockingTest {
        val item = ToolbarMenu.Item.Stop

        val controller = createController(scope = this)
        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.STOP)) }
        verify { sessionUseCases.stopLoading(currentSession) }
    }

    @Test
    fun handleToolbarSettingsPress() = runBlockingTest {
        val item = ToolbarMenu.Item.Settings

        val controller = createController(scope = this)
        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.SETTINGS)) }
        verify {
            val directions = BrowserFragmentDirections.actionBrowserFragmentToSettingsFragment()
            navController.nav(R.id.browserFragment, directions)
        }
    }

    @Test
    fun handleToolbarBookmarkPress() = runBlockingTest {
        val item = ToolbarMenu.Item.Bookmark

        val controller = createController(scope = this)
        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.BOOKMARK)) }
        verify { bookmarkTapped(currentSession) }
    }

    @Test
    fun handleToolbarBookmarksPress() = runBlockingTest {
        val item = ToolbarMenu.Item.Bookmarks

        val controller = createController(scope = this)
        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.BOOKMARKS)) }
        verify {
            val directions = BrowserFragmentDirections.actionGlobalBookmarkFragment(BookmarkRoot.Mobile.id)
            navController.nav(R.id.browserFragment, directions)
        }
    }

    @Test
    fun handleToolbarHistoryPress() = runBlockingTest {
        val item = ToolbarMenu.Item.History

        val controller = createController(scope = this)
        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.HISTORY)) }
        verify {
            val directions = BrowserFragmentDirections.actionGlobalHistoryFragment()
            navController.nav(R.id.browserFragment, directions)
        }
    }

    @Test
    fun handleToolbarRequestDesktopOnPress() = runBlockingTest {
        val requestDesktopSiteUseCase: SessionUseCases.RequestDesktopSiteUseCase =
            mockk(relaxed = true)
        val item = ToolbarMenu.Item.RequestDesktop(true)

        every { sessionUseCases.requestDesktopSite } returns requestDesktopSiteUseCase

        val controller = createController(scope = this)
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
    fun handleToolbarRequestDesktopOffPress() = runBlockingTest {
        val requestDesktopSiteUseCase: SessionUseCases.RequestDesktopSiteUseCase =
            mockk(relaxed = true)
        val item = ToolbarMenu.Item.RequestDesktop(false)

        every { sessionUseCases.requestDesktopSite } returns requestDesktopSiteUseCase

        val controller = createController(scope = this)
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
    fun handleToolbarAddToTopSitesPressed() = runBlockingTest {
        val item = ToolbarMenu.Item.AddToTopSites
        every {
            swipeRefreshLayout.context.getString(R.string.snackbar_added_to_top_sites)
        } returns "Added to top sites!"

        val controller = createController(scope = this)
        controller.handleToolbarItemInteraction(item)

        verify { topSiteStorage.addTopSite(currentSession.title, currentSession.url) }
        verify { snackbar.setText("Added to top sites!") }
        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.ADD_TO_TOP_SITES)) }
    }

    @Test
    fun handleToolbarAddonsManagerPress() = runBlockingTest {
        val item = ToolbarMenu.Item.AddonsManager

        val controller = createController(scope = this)
        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.ADDONS_MANAGER)) }
    }

    @Test
    fun handleToolbarAddToHomeScreenPress() = runBlockingTest {
        val item = ToolbarMenu.Item.AddToHomeScreen

        val controller = createController(scope = this)
        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.ADD_TO_HOMESCREEN)) }
    }

    @Test
    fun handleToolbarSharePress() = runBlockingTest {
        val item = ToolbarMenu.Item.Share

        every { currentSession.url } returns "https://mozilla.org"

        val controller = createController(scope = this)
        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.SHARE)) }
        verify { navController.navigate(any<NavDirections>()) }
    }

    @Test
    fun handleToolbarFindInPagePress() = runBlockingTest {
        val item = ToolbarMenu.Item.FindInPage

        val controller = createController(scope = this)
        controller.handleToolbarItemInteraction(item)

        verify { findInPageLauncher() }
        verify { metrics.track(Event.FindInPageOpened) }
    }

    @Test
    fun handleToolbarSaveToCollectionPressWhenAtLeastOneCollectionExists() = runBlockingTest {
        val item = ToolbarMenu.Item.SaveToCollection
        val cachedTabCollections: List<TabCollection> = mockk(relaxed = true)
        every { activity.components.useCases.sessionUseCases } returns sessionUseCases
        every { activity.components.core.tabCollectionStorage.cachedTabCollections } returns cachedTabCollections

        val controller = createController(scope = this)
        controller.handleToolbarItemInteraction(item)

        verify {
            metrics.track(
                Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.SAVE_TO_COLLECTION)
            )
        }
        verify {
            metrics.track(
                Event.CollectionSaveButtonPressed(DefaultBrowserToolbarController.TELEMETRY_BROWSER_IDENTIFIER)
            )
        }
        verify {
            val directions =
                BrowserFragmentDirections.actionGlobalCollectionCreationFragment(
                    saveCollectionStep = SaveCollectionStep.SelectCollection,
                    tabIds = arrayOf(currentSession.id),
                    selectedTabIds = arrayOf(currentSession.id)
                )
            navController.nav(R.id.browserFragment, directions)
        }
    }

    @Test
    fun handleToolbarSaveToCollectionPressWhenNoCollectionsExists() = runBlockingTest {
        val item = ToolbarMenu.Item.SaveToCollection
        val cachedTabCollectionsEmpty: List<TabCollection> = emptyList()
        every { activity.components.useCases.sessionUseCases } returns sessionUseCases
        every { activity.components.core.tabCollectionStorage.cachedTabCollections } returns cachedTabCollectionsEmpty

        val controller = createController(scope = this)
        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.SAVE_TO_COLLECTION)) }
        verify {
            metrics.track(
                Event.CollectionSaveButtonPressed(
                    DefaultBrowserToolbarController.TELEMETRY_BROWSER_IDENTIFIER
                )
            )
        }
        verify {
            val directions =
                BrowserFragmentDirections.actionGlobalCollectionCreationFragment(
                    saveCollectionStep = SaveCollectionStep.NameCollection,
                    tabIds = arrayOf(currentSession.id),
                    selectedTabIds = arrayOf(currentSession.id)
                )
            navController.nav(R.id.browserFragment, directions)
        }
    }

    @Test
    fun handleToolbarOpenInFenixPress() = runBlockingTest {
        val controller = createController(scope = this, customTabSession = currentSession)

        val item = ToolbarMenu.Item.OpenInFenix

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
    fun handleToolbarQuitPress() = runBlockingTest {
        val item = ToolbarMenu.Item.Quit
        val testScope = this

        val controller = createController(scope = testScope)

        controller.handleToolbarItemInteraction(item)

        verify { deleteAndQuit(activity, testScope, null) }
    }

    @Test
    fun handleToolbarCloseTabPressWithLastPrivateSession() = runBlockingTest {
        every { currentSession.id } returns "1"
        val browsingModeManager = object : BrowsingModeManager {
            override var mode = BrowsingMode.Private
        }
        val item = TabCounterMenuItem.CloseTab
        val sessions = listOf(
            mockk<Session> {
                every { private } returns true
            }
        )

        every { currentSession.private } returns true
        every { sessionManager.sessions } returns sessions
        every { activity.browsingModeManager } returns browsingModeManager

        val controller = createController(scope = this)
        controller.handleTabCounterItemInteraction(item)
        verify { navController.navigate(BrowserFragmentDirections.actionGlobalHome(sessionToDelete = "1")) }
        assertEquals(BrowsingMode.Normal, browsingModeManager.mode)
    }

    @Test
    fun handleToolbarCloseTabPress() = runBlockingTest {
        val tabsUseCases: TabsUseCases = mockk(relaxed = true)
        val removeTabUseCase: TabsUseCases.RemoveTabUseCase = mockk(relaxed = true)
        val item = TabCounterMenuItem.CloseTab

        every { sessionManager.sessions } returns emptyList()
        every { activity.components.useCases.tabsUseCases } returns tabsUseCases
        every { tabsUseCases.removeTab } returns removeTabUseCase

        val controller = createController(scope = this)
        controller.handleTabCounterItemInteraction(item)
        verify { removeTabUseCase.invoke(currentSession) }
    }

    @Test
    fun handleToolbarNewTabPress() = runBlockingTest {
        val browsingModeManager: BrowsingModeManager = DefaultBrowsingModeManager(BrowsingMode.Private) {}
        val item = TabCounterMenuItem.NewTab(false)

        every { activity.browsingModeManager } returns browsingModeManager

        val controller = createController(scope = this)
        controller.handleTabCounterItemInteraction(item)
        assertEquals(BrowsingMode.Normal, activity.browsingModeManager.mode)
        verify { navController.popBackStack(R.id.homeFragment, false) }
    }

    @Test
    fun handleToolbarNewPrivateTabPress() = runBlockingTest {
        val browsingModeManager: BrowsingModeManager = DefaultBrowsingModeManager(BrowsingMode.Normal) {}
        val item = TabCounterMenuItem.NewTab(true)

        every { activity.browsingModeManager } returns browsingModeManager

        val controller = createController(scope = this)
        controller.handleTabCounterItemInteraction(item)
        assertEquals(BrowsingMode.Private, activity.browsingModeManager.mode)
        verify { navController.popBackStack(R.id.homeFragment, false) }
    }

    private fun createController(
        scope: CoroutineScope,
        activity: HomeActivity = this.activity,
        customTabSession: Session? = null
    ) = DefaultBrowserToolbarController(
        activity = activity,
        navController = navController,
        findInPageLauncher = findInPageLauncher,
        engineView = engineView,
        browserAnimator = browserAnimator,
        customTabSession = customTabSession,
        openInFenixIntent = openInFenixIntent,
        scope = scope,
        swipeRefresh = swipeRefreshLayout,
        tabCollectionStorage = tabCollectionStorage,
        topSiteStorage = topSiteStorage,
        bookmarkTapped = bookmarkTapped,
        readerModeController = readerModeController,
        sessionManager = sessionManager,
        onTabCounterClicked = onTabCounterClicked,
        onCloseTab = onCloseTab
    ).apply {
        ioScope = scope
    }
}
