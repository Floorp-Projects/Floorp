/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Intent
import androidx.navigation.NavController
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
import io.mockk.unmockkObject
import io.mockk.unmockkStatic
import io.mockk.verify
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runBlockingTest
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.browser.state.action.CustomTabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ReaderState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.feature.search.SearchUseCases
import mozilla.components.feature.session.SessionFeature
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.tabs.CustomTabsUseCases
import mozilla.components.feature.top.sites.DefaultTopSitesStorage
import mozilla.components.feature.top.sites.TopSitesUseCases
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserAnimator
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.browser.readermode.ReaderModeController
import org.mozilla.fenix.collections.SaveCollectionStep
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.directionsEq
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.settings.deletebrowsingdata.deleteAndQuit
import org.mozilla.fenix.utils.Settings

@OptIn(ExperimentalCoroutinesApi::class)
@RunWith(FenixRobolectricTestRunner::class)
@Suppress("ForbiddenComment")
class DefaultBrowserToolbarMenuControllerTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @MockK private lateinit var swipeRefreshLayout: SwipeRefreshLayout
    @RelaxedMockK private lateinit var activity: HomeActivity
    @RelaxedMockK private lateinit var navController: NavController
    @RelaxedMockK private lateinit var findInPageLauncher: () -> Unit
    @RelaxedMockK private lateinit var bookmarkTapped: (String, String) -> Unit
    @RelaxedMockK private lateinit var openInFenixIntent: Intent
    @RelaxedMockK private lateinit var metrics: MetricController
    @RelaxedMockK private lateinit var settings: Settings
    @RelaxedMockK private lateinit var searchUseCases: SearchUseCases
    @RelaxedMockK private lateinit var sessionUseCases: SessionUseCases
    @RelaxedMockK private lateinit var customTabUseCases: CustomTabsUseCases
    @RelaxedMockK private lateinit var browserAnimator: BrowserAnimator
    @RelaxedMockK private lateinit var snackbar: FenixSnackbar
    @RelaxedMockK private lateinit var tabCollectionStorage: TabCollectionStorage
    @RelaxedMockK private lateinit var topSitesUseCase: TopSitesUseCases
    @RelaxedMockK private lateinit var readerModeController: ReaderModeController
    @MockK private lateinit var sessionFeatureWrapper: ViewBoundFeatureWrapper<SessionFeature>
    @RelaxedMockK private lateinit var sessionFeature: SessionFeature
    @RelaxedMockK private lateinit var topSitesStorage: DefaultTopSitesStorage

    private lateinit var browserStore: BrowserStore
    private lateinit var selectedTab: TabSessionState

    @Before
    fun setUp() {
        MockKAnnotations.init(this)

        mockkStatic(
            "org.mozilla.fenix.settings.deletebrowsingdata.DeleteAndQuitKt"
        )
        every { deleteAndQuit(any(), any(), any()) } just Runs

        mockkObject(FenixSnackbar.Companion)
        every { FenixSnackbar.make(any(), any(), any(), any()) } returns snackbar

        every { activity.components.useCases.sessionUseCases } returns sessionUseCases
        every { activity.components.useCases.customTabsUseCases } returns customTabUseCases
        every { activity.components.useCases.searchUseCases } returns searchUseCases
        every { activity.components.useCases.topSitesUseCase } returns topSitesUseCase
        every { sessionFeatureWrapper.get() } returns sessionFeature
        every { navController.currentDestination } returns mockk {
            every { id } returns R.id.browserFragment
        }
        every { settings.topSitesMaxLimit } returns 16

        val onComplete = slot<() -> Unit>()
        every { browserAnimator.captureEngineViewAndDrawStatically(capture(onComplete)) } answers { onComplete.captured.invoke() }

        selectedTab = createTab("https://www.mozilla.org", id = "1")
        browserStore = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(selectedTab),
                selectedTabId = selectedTab.id
            )
        )
    }

    @After
    fun tearDown() {
        unmockkStatic("org.mozilla.fenix.settings.deletebrowsingdata.DeleteAndQuitKt")
        unmockkObject(FenixSnackbar.Companion)
    }

    // TODO: These can be removed for https://github.com/mozilla-mobile/fenix/issues/17870
    // todo === Start ===
    @Test
    fun handleToolbarBookmarkPressWithReaderModeInactive() = runBlockingTest {
        if (!FeatureFlags.toolbarMenuFeature) {
            val item = ToolbarMenu.Item.Bookmark

            val title = "Mozilla"
            val url = "https://mozilla.org"
            val regularTab = createTab(
                url = url,
                readerState = ReaderState(active = false, activeUrl = "https://1234.org"),
                title = title
            )
            val store =
                BrowserStore(BrowserState(tabs = listOf(regularTab), selectedTabId = regularTab.id))

            val controller = createController(scope = this, store = store)
            controller.handleToolbarItemInteraction(item)

            verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.BOOKMARK)) }
            verify { bookmarkTapped(url, title) }
        }
    }

    @Test
    fun `IF reader mode is active WHEN bookmark menu item is pressed THEN menu item is handled`() = runBlockingTest {
        if (!FeatureFlags.toolbarMenuFeature) {
            val item = ToolbarMenu.Item.Bookmark
            val title = "Mozilla"
            val readerUrl = "moz-extension://1234"
            val readerTab = createTab(
                url = readerUrl,
                readerState = ReaderState(active = true, activeUrl = "https://mozilla.org"),
                title = title
            )
            browserStore =
                BrowserStore(BrowserState(tabs = listOf(readerTab), selectedTabId = readerTab.id))

            val controller = createController(scope = this, store = browserStore)
            controller.handleToolbarItemInteraction(item)

            verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.BOOKMARK)) }
            verify { bookmarkTapped("https://mozilla.org", title) }
        }
    }

    @Test
    fun `WHEN open in Fenix menu item is pressed THEN menu item is handled correctly`() = runBlockingTest {
        if (!FeatureFlags.toolbarMenuFeature) {

            val customTab = createCustomTab("https://mozilla.org")
            browserStore.dispatch(CustomTabListAction.AddCustomTabAction(customTab)).joinBlocking()
            val controller = createController(
                scope = this,
                store = browserStore,
                customTabSessionId = customTab.id
            )

            val item = ToolbarMenu.Item.OpenInFenix

            every { activity.startActivity(any()) } just Runs
            controller.handleToolbarItemInteraction(item)

            verify { sessionFeature.release() }
            verify { customTabUseCases.migrate(customTab.id, true) }
            verify { activity.startActivity(openInFenixIntent) }
            verify { activity.finishAndRemoveTask() }
        }
    }

    @Test
    fun `WHEN quit menu item is pressed THEN menu item is handled correctly`() = runBlockingTest {
        if (!FeatureFlags.toolbarMenuFeature) {
            val item = ToolbarMenu.Item.Quit
            val testScope = this

            val controller = createController(scope = this, store = browserStore)

            controller.handleToolbarItemInteraction(item)

            verify { deleteAndQuit(activity, testScope, null) }
        }
    }

    @Test
    fun handleToolbarOpenInAppPress() = runBlockingTest {
        if (!FeatureFlags.toolbarMenuFeature) {
            val item = ToolbarMenu.Item.OpenInApp

            val controller = createController(scope = this, store = browserStore)

            controller.handleToolbarItemInteraction(item)

            verify { settings.openInAppOpened = true }
        }
    }

    @Test
    fun `WHEN reader mode menu item is pressed THEN handle appearance change`() = runBlockingTest {
        val item = ToolbarMenu.Item.CustomizeReaderView

        val controller = createController(scope = this, store = browserStore)

        controller.handleToolbarItemInteraction(item)

        verify { readerModeController.showControls() }
        verify { metrics.track(Event.ReaderModeAppearanceOpened) }
    }
    // todo === End ===

    @Test
    fun `WHEN backwards nav menu item is pressed THEN the session navigates back with active session`() = runBlockingTest {
        val item = ToolbarMenu.Item.Back(false)

        val controller = createController(scope = this, store = browserStore)
        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.BACK)) }
        verify { sessionUseCases.goBack(browserStore.state.selectedTabId!!) }
    }

    @Test
    fun `WHEN backwards nav menu item is long pressed THEN the session navigates back with no active session`() = runBlockingTest {
        val item = ToolbarMenu.Item.Back(true)

        val controller = createController(scope = this, store = browserStore)
        controller.handleToolbarItemInteraction(item)

        val directions = BrowserFragmentDirections.actionGlobalTabHistoryDialogFragment(null)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.BACK)) }
        verify { navController.navigate(directions) }
    }

    @Test
    fun `WHEN forward nav menu item is pressed THEN the session navigates forward to active session`() = runBlockingTest {
        val item = ToolbarMenu.Item.Forward(false)

        val controller = createController(scope = this, store = browserStore)
        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.FORWARD)) }
        verify { sessionUseCases.goForward(selectedTab.id) }
    }

    @Test
    fun `WHEN forward nav menu item is long pressed THEN the browser navigates forward with no active session`() = runBlockingTest {
        val item = ToolbarMenu.Item.Forward(true)

        val controller = createController(scope = this, store = browserStore)
        controller.handleToolbarItemInteraction(item)

        val directions = BrowserFragmentDirections.actionGlobalTabHistoryDialogFragment(null)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.FORWARD)) }
        verify { navController.navigate(directions) }
    }

    @Test
    fun `WHEN reload nav menu item is pressed THEN the session reloads from cache`() = runBlockingTest {
        val item = ToolbarMenu.Item.Reload(false)

        val controller = createController(scope = this, store = browserStore)
        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.RELOAD)) }
        verify { sessionUseCases.reload(selectedTab.id) }
    }

    @Test
    fun `WHEN reload nav menu item is long pressed THEN the session reloads with no cache`() = runBlockingTest {
        val item = ToolbarMenu.Item.Reload(true)

        val controller = createController(scope = this, store = browserStore)
        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.RELOAD)) }
        verify {
            sessionUseCases.reload(
                selectedTab.id,
                EngineSession.LoadUrlFlags.select(EngineSession.LoadUrlFlags.BYPASS_CACHE)
            )
        }
    }

    @Test
    fun `WHEN stop nav menu item is pressed THEN the session stops loading`() = runBlockingTest {
        val item = ToolbarMenu.Item.Stop

        val controller = createController(scope = this, store = browserStore)
        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.STOP)) }
        verify { sessionUseCases.stopLoading(selectedTab.id) }
    }

    @Test
    fun `WHEN settings menu item is pressed THEN menu item is handled`() = runBlockingTest {
        val item = ToolbarMenu.Item.Settings

        val controller = createController(scope = this, store = browserStore)
        controller.handleToolbarItemInteraction(item)

        val directions = BrowserFragmentDirections.actionBrowserFragmentToSettingsFragment()

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.SETTINGS)) }
        verify { navController.navigate(directions, null) }
    }

    @Test
    fun `WHEN bookmark menu item is pressed THEN navigate to bookmarks page`() = runBlockingTest {
        val item = ToolbarMenu.Item.Bookmarks

        val controller = createController(scope = this, store = browserStore)
        controller.handleToolbarItemInteraction(item)

        val directions = BrowserFragmentDirections.actionGlobalBookmarkFragment(BookmarkRoot.Mobile.id)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.BOOKMARKS)) }
        verify { navController.navigate(directions, null) }
    }

    @Test
    fun `WHEN history menu item is pressed THEN navigate to history page`() = runBlockingTest {
        val item = ToolbarMenu.Item.History

        val controller = createController(scope = this, store = browserStore)
        controller.handleToolbarItemInteraction(item)

        val directions = BrowserFragmentDirections.actionGlobalHistoryFragment()

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.HISTORY)) }
        verify { navController.navigate(directions, null) }
    }

    @Test
    fun `WHEN request desktop menu item is toggled On THEN desktop site is requested for the session`() = runBlockingTest {
        val requestDesktopSiteUseCase: SessionUseCases.RequestDesktopSiteUseCase =
            mockk(relaxed = true)
        val item = ToolbarMenu.Item.RequestDesktop(true)

        every { sessionUseCases.requestDesktopSite } returns requestDesktopSiteUseCase

        val controller = createController(scope = this, store = browserStore)
        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.DESKTOP_VIEW_ON)) }
        verify {
            requestDesktopSiteUseCase.invoke(
                true,
                selectedTab.id
            )
        }
    }

    @Test
    fun `WHEN request desktop menu item is toggled Off THEN mobile site is requested for the session`() = runBlockingTest {
        val requestDesktopSiteUseCase: SessionUseCases.RequestDesktopSiteUseCase =
            mockk(relaxed = true)
        val item = ToolbarMenu.Item.RequestDesktop(false)

        every { sessionUseCases.requestDesktopSite } returns requestDesktopSiteUseCase

        val controller = createController(scope = this, store = browserStore)
        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.DESKTOP_VIEW_OFF)) }
        verify {
            requestDesktopSiteUseCase.invoke(
                false,
                selectedTab.id
            )
        }
    }

    @Test
    fun `WHEN Add To Top Sites menu item is pressed THEN add site AND show snackbar`() = runBlockingTest {
        val item = ToolbarMenu.Item.AddToTopSites
        val addPinnedSiteUseCase: TopSitesUseCases.AddPinnedSiteUseCase = mockk(relaxed = true)

        every { topSitesUseCase.addPinnedSites } returns addPinnedSiteUseCase
        every {
            swipeRefreshLayout.context.getString(R.string.snackbar_added_to_top_sites)
        } returns "Added to top sites!"

        val controller = createController(scope = this, store = browserStore)
        controller.handleToolbarItemInteraction(item)

        verify { addPinnedSiteUseCase.invoke(selectedTab.content.title, selectedTab.content.url) }
        verify { snackbar.setText("Added to top sites!") }
        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.ADD_TO_TOP_SITES)) }
    }

    @Test
    fun `WHEN addon extensions menu item is pressed THEN navigate to addons manager`() = runBlockingTest {
        val item = ToolbarMenu.Item.AddonsManager

        val controller = createController(scope = this, store = browserStore)
        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.ADDONS_MANAGER)) }
    }

    @Test
    fun `WHEN Add To Home Screen menu item is pressed THEN add site`() = runBlockingTest {
        val item = ToolbarMenu.Item.AddToHomeScreen

        val controller = createController(scope = this, store = browserStore)
        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.ADD_TO_HOMESCREEN)) }
    }

    @Test
    fun `IF reader mode is inactive WHEN share menu item is pressed THEN navigate to share screen`() = runBlockingTest {
        val item = ToolbarMenu.Item.Share
        val title = "Mozilla"
        val url = "https://mozilla.org"
        val regularTab = createTab(
            url = url,
            readerState = ReaderState(active = false, activeUrl = "https://1234.org"),
            title = title
        )
        browserStore = BrowserStore(BrowserState(tabs = listOf(regularTab), selectedTabId = regularTab.id))
        val controller = createController(scope = this, store = browserStore)
        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.SHARE)) }
        verify {
            navController.navigate(
                directionsEq(
                    NavGraphDirections.actionGlobalShareFragment(
                        data = arrayOf(ShareData(url = "https://mozilla.org", title = "Mozilla")),
                        showPage = true
                    )
                )
            )
        }
    }

    @Test
    fun `IF reader mode is active WHEN share menu item is pressed THEN navigate to share screen`() = runBlockingTest {
        val item = ToolbarMenu.Item.Share
        val title = "Mozilla"
        val readerUrl = "moz-extension://1234"
        val readerTab = createTab(
            url = readerUrl,
            readerState = ReaderState(active = true, activeUrl = "https://mozilla.org"),
            title = title
        )
        browserStore = BrowserStore(BrowserState(tabs = listOf(readerTab), selectedTabId = readerTab.id))
        val controller = createController(scope = this, store = browserStore)
        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.SHARE)) }
        verify {
            navController.navigate(
                directionsEq(
                    NavGraphDirections.actionGlobalShareFragment(
                        data = arrayOf(ShareData(url = "https://mozilla.org", title = "Mozilla")),
                        showPage = true
                    )
                )
            )
        }
    }

    @Test
    fun `WHEN Find In Page menu item is pressed THEN launch finder`() = runBlockingTest {
        val item = ToolbarMenu.Item.FindInPage

        val controller = createController(scope = this, store = browserStore)
        controller.handleToolbarItemInteraction(item)

        verify { findInPageLauncher() }
        verify { metrics.track(Event.FindInPageOpened) }
    }

    @Test
    fun `IF one or more collection exists WHEN Save To Collection menu item is pressed THEN navigate to save collection page`() = runBlockingTest {
        val item = ToolbarMenu.Item.SaveToCollection
        val cachedTabCollections: List<TabCollection> = mockk(relaxed = true)
        every { tabCollectionStorage.cachedTabCollections } returns cachedTabCollections

        val controller = createController(scope = this, store = browserStore)
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

        val directions = BrowserFragmentDirections.actionGlobalCollectionCreationFragment(
            saveCollectionStep = SaveCollectionStep.SelectCollection,
            tabIds = arrayOf(selectedTab.id),
            selectedTabIds = arrayOf(selectedTab.id)
        )
        verify { navController.navigate(directionsEq(directions), null) }
    }

    @Test
    fun `IF no collection exists WHEN Save To Collection menu item is pressed THEN navigate to create collection page`() = runBlockingTest {
        val item = ToolbarMenu.Item.SaveToCollection
        val cachedTabCollectionsEmpty: List<TabCollection> = emptyList()
        every { tabCollectionStorage.cachedTabCollections } returns cachedTabCollectionsEmpty

        val controller = createController(scope = this, store = browserStore)
        controller.handleToolbarItemInteraction(item)

        verify { metrics.track(Event.BrowserMenuItemTapped(Event.BrowserMenuItemTapped.Item.SAVE_TO_COLLECTION)) }
        verify {
            metrics.track(
                Event.CollectionSaveButtonPressed(
                    DefaultBrowserToolbarController.TELEMETRY_BROWSER_IDENTIFIER
                )
            )
        }
        val directions = BrowserFragmentDirections.actionGlobalCollectionCreationFragment(
            saveCollectionStep = SaveCollectionStep.NameCollection,
            tabIds = arrayOf(selectedTab.id),
            selectedTabIds = arrayOf(selectedTab.id)
        )
        verify { navController.navigate(directionsEq(directions), null) }
    }

    @Test
    fun `WHEN New Tab menu item is pressed THEN navigate to a new tab home`() = runBlockingTest {
        val item = ToolbarMenu.Item.NewTab

        val controller = createController(scope = this, store = browserStore)

        controller.handleToolbarItemInteraction(item)

        verify {
            navController.navigate(
                directionsEq(
                    NavGraphDirections.actionGlobalHome(
                        focusOnAddressBar = true
                    )
                )
            )
        }
    }

    private fun createController(
        scope: CoroutineScope,
        store: BrowserStore,
        activity: HomeActivity = this.activity,
        customTabSessionId: String? = null
    ) = DefaultBrowserToolbarMenuController(
        store = store,
        activity = activity,
        navController = navController,
        metrics = metrics,
        settings = settings,
        findInPageLauncher = findInPageLauncher,
        browserAnimator = browserAnimator,
        customTabSessionId = customTabSessionId,
        openInFenixIntent = openInFenixIntent,
        scope = scope,
        swipeRefresh = swipeRefreshLayout,
        tabCollectionStorage = tabCollectionStorage,
        bookmarkTapped = bookmarkTapped,
        readerModeController = readerModeController,
        sessionFeature = sessionFeatureWrapper,
        topSitesStorage = topSitesStorage,
        browserStore = browserStore
    ).apply {
        ioScope = scope
    }
}
