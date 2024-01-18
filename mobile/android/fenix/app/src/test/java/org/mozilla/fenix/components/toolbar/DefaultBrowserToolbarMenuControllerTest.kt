/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Intent
import android.view.ViewGroup
import androidx.navigation.NavController
import io.mockk.MockKAnnotations
import io.mockk.Runs
import io.mockk.coEvery
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
import kotlinx.coroutines.test.runTest
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
import mozilla.components.feature.top.sites.PinnedSiteStorage
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.feature.top.sites.TopSitesUseCases
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.Collections
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.GleanMetrics.ReaderMode
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserAnimator
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.browser.readermode.ReaderModeController
import org.mozilla.fenix.collections.SaveCollectionStep
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.components.accounts.AccountState
import org.mozilla.fenix.components.accounts.FenixFxAEntryPoint
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.directionsEq
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.settings.deletebrowsingdata.deleteAndQuit
import org.mozilla.fenix.utils.Settings

@OptIn(ExperimentalCoroutinesApi::class)
@RunWith(FenixRobolectricTestRunner::class)
class DefaultBrowserToolbarMenuControllerTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    @MockK private lateinit var snackbarParent: ViewGroup

    @RelaxedMockK private lateinit var activity: HomeActivity

    @RelaxedMockK private lateinit var navController: NavController

    @RelaxedMockK private lateinit var openInFenixIntent: Intent

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

    @RelaxedMockK private lateinit var pinnedSiteStorage: PinnedSiteStorage

    private lateinit var browserStore: BrowserStore
    private lateinit var selectedTab: TabSessionState

    @Before
    fun setUp() {
        MockKAnnotations.init(this)

        mockkStatic(
            "org.mozilla.fenix.settings.deletebrowsingdata.DeleteAndQuitKt",
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

        val onComplete = slot<(Boolean) -> Unit>()
        every { browserAnimator.captureEngineViewAndDrawStatically(any(), capture(onComplete)) } answers { onComplete.captured.invoke(true) }

        selectedTab = createTab("https://www.mozilla.org", id = "1")
        browserStore = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(selectedTab),
                selectedTabId = selectedTab.id,
            ),
        )
    }

    @After
    fun tearDown() {
        unmockkStatic("org.mozilla.fenix.settings.deletebrowsingdata.DeleteAndQuitKt")
        unmockkObject(FenixSnackbar.Companion)
    }

    @Test
    fun handleToolbarBookmarkPressWithReaderModeInactive() = runTest {
        val item = ToolbarMenu.Item.Bookmark

        val expectedTitle = "Mozilla"
        val expectedUrl = "https://mozilla.org"
        val regularTab = createTab(
            url = expectedUrl,
            readerState = ReaderState(active = false, activeUrl = "https://1234.org"),
            title = expectedTitle,
        )
        val store =
            BrowserStore(BrowserState(tabs = listOf(regularTab), selectedTabId = regularTab.id))

        var bookmarkTappedInvoked = false
        val controller = createController(
            scope = this,
            store = store,
            bookmarkTapped = { url, title ->
                assertEquals(expectedTitle, title)
                assertEquals(expectedUrl, url)
                bookmarkTappedInvoked = true
            },
        )
        assertNull(Events.browserMenuAction.testGetValue())

        controller.handleToolbarItemInteraction(item)

        assertNotNull(Events.browserMenuAction.testGetValue())
        val snapshot = Events.browserMenuAction.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("bookmark", snapshot.single().extra?.getValue("item"))

        assertTrue(bookmarkTappedInvoked)
    }

    @Test
    fun `IF reader mode is active WHEN bookmark menu item is pressed THEN menu item is handled`() = runTest {
        val item = ToolbarMenu.Item.Bookmark
        val expectedTitle = "Mozilla"
        val readerUrl = "moz-extension://1234"
        val readerTab = createTab(
            url = readerUrl,
            readerState = ReaderState(active = true, activeUrl = "https://mozilla.org"),
            title = expectedTitle,
        )
        browserStore =
            BrowserStore(BrowserState(tabs = listOf(readerTab), selectedTabId = readerTab.id))

        var bookmarkTappedInvoked = false
        val controller = createController(
            scope = this,
            store = browserStore,
            bookmarkTapped = { url, title ->
                assertEquals(expectedTitle, title)
                assertEquals(readerTab.readerState.activeUrl, url)
                bookmarkTappedInvoked = true
            },
        )
        assertNull(Events.browserMenuAction.testGetValue())

        controller.handleToolbarItemInteraction(item)

        assertNotNull(Events.browserMenuAction.testGetValue())
        val snapshot = Events.browserMenuAction.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("bookmark", snapshot.single().extra?.getValue("item"))

        assertTrue(bookmarkTappedInvoked)
    }

    @Test
    fun `WHEN open in Fenix menu item is pressed THEN menu item is handled correctly`() = runTest {
        val customTab = createCustomTab("https://mozilla.org")
        browserStore.dispatch(CustomTabListAction.AddCustomTabAction(customTab)).joinBlocking()
        val controller = createController(
            scope = this,
            store = browserStore,
            customTabSessionId = customTab.id,
        )

        val item = ToolbarMenu.Item.OpenInFenix

        every { activity.startActivity(any()) } just Runs
        controller.handleToolbarItemInteraction(item)

        verify { sessionFeature.release() }
        verify { customTabUseCases.migrate(customTab.id, true) }
        verify { activity.startActivity(openInFenixIntent) }
        verify { activity.finishAndRemoveTask() }
    }

    @Test
    fun `WHEN reader mode menu item is pressed THEN handle appearance change`() = runTest {
        val item = ToolbarMenu.Item.CustomizeReaderView
        assertNull(ReaderMode.appearance.testGetValue())

        val controller = createController(scope = this, store = browserStore)

        controller.handleToolbarItemInteraction(item)

        verify { readerModeController.showControls() }
        assertNotNull(ReaderMode.appearance.testGetValue())
        assertNull(ReaderMode.appearance.testGetValue()!!.single().extra)
    }

    @Test
    fun `WHEN quit menu item is pressed THEN menu item is handled correctly`() = runTest {
        val item = ToolbarMenu.Item.Quit
        val testScope = this

        val controller = createController(scope = this, store = browserStore)

        controller.handleToolbarItemInteraction(item)

        verify { deleteAndQuit(activity, testScope, null) }
    }

    @Test
    fun `WHEN backwards nav menu item is pressed THEN the session navigates back with active session`() = runTest {
        val item = ToolbarMenu.Item.Back(false)

        val controller = createController(scope = this, store = browserStore)
        assertNull(Events.browserMenuAction.testGetValue())

        controller.handleToolbarItemInteraction(item)

        assertNotNull(Events.browserMenuAction.testGetValue())
        val snapshot = Events.browserMenuAction.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("back", snapshot.single().extra?.getValue("item"))

        verify { sessionUseCases.goBack(browserStore.state.selectedTabId!!) }
    }

    @Test
    fun `WHEN backwards nav menu item is long pressed THEN the session navigates back with no active session`() = runTest {
        val item = ToolbarMenu.Item.Back(true)

        val controller = createController(scope = this, store = browserStore)
        assertNull(Events.browserMenuAction.testGetValue())

        controller.handleToolbarItemInteraction(item)

        assertNotNull(Events.browserMenuAction.testGetValue())
        val snapshot = Events.browserMenuAction.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("back_long_press", snapshot.single().extra?.getValue("item"))
        val directions = BrowserFragmentDirections.actionGlobalTabHistoryDialogFragment(null)

        verify { navController.navigate(directions) }
    }

    @Test
    fun `WHEN forward nav menu item is pressed THEN the session navigates forward to active session`() = runTest {
        val item = ToolbarMenu.Item.Forward(false)

        val controller = createController(scope = this, store = browserStore)
        assertNull(Events.browserMenuAction.testGetValue())

        controller.handleToolbarItemInteraction(item)

        assertNotNull(Events.browserMenuAction.testGetValue())
        val snapshot = Events.browserMenuAction.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("forward", snapshot.single().extra?.getValue("item"))

        verify { sessionUseCases.goForward(selectedTab.id) }
    }

    @Test
    fun `WHEN forward nav menu item is long pressed THEN the browser navigates forward with no active session`() = runTest {
        val item = ToolbarMenu.Item.Forward(true)

        val controller = createController(scope = this, store = browserStore)
        assertNull(Events.browserMenuAction.testGetValue())

        controller.handleToolbarItemInteraction(item)

        assertNotNull(Events.browserMenuAction.testGetValue())
        val snapshot = Events.browserMenuAction.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("forward_long_press", snapshot.single().extra?.getValue("item"))

        val directions = BrowserFragmentDirections.actionGlobalTabHistoryDialogFragment(null)

        verify { navController.navigate(directions) }
    }

    @Test
    fun `WHEN reload nav menu item is pressed THEN the session reloads from cache`() = runTest {
        val item = ToolbarMenu.Item.Reload(false)

        val controller = createController(scope = this, store = browserStore)
        assertNull(Events.browserMenuAction.testGetValue())

        controller.handleToolbarItemInteraction(item)

        assertNotNull(Events.browserMenuAction.testGetValue())
        val snapshot = Events.browserMenuAction.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("reload", snapshot.single().extra?.getValue("item"))

        verify { sessionUseCases.reload(selectedTab.id) }
    }

    @Test
    fun `WHEN reload nav menu item is long pressed THEN the session reloads with no cache`() = runTest {
        val item = ToolbarMenu.Item.Reload(true)

        val controller = createController(scope = this, store = browserStore)
        assertNull(Events.browserMenuAction.testGetValue())

        controller.handleToolbarItemInteraction(item)

        assertNotNull(Events.browserMenuAction.testGetValue())
        val snapshot = Events.browserMenuAction.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("reload", snapshot.single().extra?.getValue("item"))

        verify {
            sessionUseCases.reload(
                selectedTab.id,
                EngineSession.LoadUrlFlags.select(EngineSession.LoadUrlFlags.BYPASS_CACHE),
            )
        }
    }

    @Test
    fun `WHEN stop nav menu item is pressed THEN the session stops loading`() = runTest {
        val item = ToolbarMenu.Item.Stop

        val controller = createController(scope = this, store = browserStore)
        assertNull(Events.browserMenuAction.testGetValue())

        controller.handleToolbarItemInteraction(item)

        assertNotNull(Events.browserMenuAction.testGetValue())
        val snapshot = Events.browserMenuAction.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("stop", snapshot.single().extra?.getValue("item"))

        verify { sessionUseCases.stopLoading(selectedTab.id) }
    }

    @Test
    fun `WHEN settings menu item is pressed THEN menu item is handled`() = runTest {
        val item = ToolbarMenu.Item.Settings

        val controller = createController(scope = this, store = browserStore)
        assertNull(Events.browserMenuAction.testGetValue())

        controller.handleToolbarItemInteraction(item)

        assertNotNull(Events.browserMenuAction.testGetValue())
        val snapshot = Events.browserMenuAction.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("settings", snapshot.single().extra?.getValue("item"))
        val directions = BrowserFragmentDirections.actionBrowserFragmentToSettingsFragment()

        verify { navController.navigate(directions, null) }
    }

    @Test
    fun `WHEN bookmark menu item is pressed THEN navigate to bookmarks page`() = runTest {
        val item = ToolbarMenu.Item.Bookmarks

        val controller = createController(scope = this, store = browserStore)
        assertNull(Events.browserMenuAction.testGetValue())

        controller.handleToolbarItemInteraction(item)

        assertNotNull(Events.browserMenuAction.testGetValue())
        val snapshot = Events.browserMenuAction.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("bookmarks", snapshot.single().extra?.getValue("item"))
        val directions = BrowserFragmentDirections.actionGlobalBookmarkFragment(BookmarkRoot.Mobile.id)

        verify { navController.navigate(directions, null) }
    }

    @Test
    fun `WHEN history menu item is pressed THEN navigate to history page`() = runTest {
        val item = ToolbarMenu.Item.History

        val controller = createController(scope = this, store = browserStore)
        assertNull(Events.browserMenuAction.testGetValue())

        controller.handleToolbarItemInteraction(item)

        assertNotNull(Events.browserMenuAction.testGetValue())
        val snapshot = Events.browserMenuAction.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("history", snapshot.single().extra?.getValue("item"))
        val directions = BrowserFragmentDirections.actionGlobalHistoryFragment()

        verify { navController.navigate(directions, null) }
    }

    @Test
    fun `WHEN request desktop menu item is toggled On THEN desktop site is requested for the session`() = runTest {
        val requestDesktopSiteUseCase: SessionUseCases.RequestDesktopSiteUseCase =
            mockk(relaxed = true)
        val item = ToolbarMenu.Item.RequestDesktop(true)

        every { sessionUseCases.requestDesktopSite } returns requestDesktopSiteUseCase

        val controller = createController(scope = this, store = browserStore)
        assertNull(Events.browserMenuAction.testGetValue())

        controller.handleToolbarItemInteraction(item)

        assertNotNull(Events.browserMenuAction.testGetValue())
        val snapshot = Events.browserMenuAction.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("desktop_view_on", snapshot.single().extra?.getValue("item"))

        verify {
            requestDesktopSiteUseCase.invoke(
                true,
                selectedTab.id,
            )
        }
    }

    @Test
    fun `WHEN request desktop menu item is toggled Off THEN mobile site is requested for the session`() = runTest {
        val requestDesktopSiteUseCase: SessionUseCases.RequestDesktopSiteUseCase =
            mockk(relaxed = true)
        val item = ToolbarMenu.Item.RequestDesktop(false)

        every { sessionUseCases.requestDesktopSite } returns requestDesktopSiteUseCase

        val controller = createController(scope = this, store = browserStore)
        assertNull(Events.browserMenuAction.testGetValue())

        controller.handleToolbarItemInteraction(item)

        assertNotNull(Events.browserMenuAction.testGetValue())
        val snapshot = Events.browserMenuAction.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("desktop_view_off", snapshot.single().extra?.getValue("item"))

        verify {
            requestDesktopSiteUseCase.invoke(
                false,
                selectedTab.id,
            )
        }
    }

    @Test
    fun `WHEN add to shortcuts menu item is pressed THEN add site AND show snackbar`() = runTestOnMain {
        val item = ToolbarMenu.Item.AddToTopSites
        val addPinnedSiteUseCase: TopSitesUseCases.AddPinnedSiteUseCase = mockk(relaxed = true)

        every { topSitesUseCase.addPinnedSites } returns addPinnedSiteUseCase
        every {
            snackbarParent.context.getString(R.string.snackbar_added_to_shortcuts)
        } returns "Added to shortcuts!"

        val controller = createController(scope = this, store = browserStore)
        assertNull(Events.browserMenuAction.testGetValue())

        controller.handleToolbarItemInteraction(item)

        assertNotNull(Events.browserMenuAction.testGetValue())
        val snapshot = Events.browserMenuAction.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("add_to_top_sites", snapshot.single().extra?.getValue("item"))

        verify { addPinnedSiteUseCase.invoke(selectedTab.content.title, selectedTab.content.url) }
        verify { snackbar.setText("Added to shortcuts!") }
    }

    @Test
    fun `GIVEN a shortcut page is open WHEN remove from shortcuts is pressed THEN show snackbar`() = runTestOnMain {
        val snackbarMessage = "Site removed"
        val item = ToolbarMenu.Item.RemoveFromTopSites
        val removePinnedSiteUseCase: TopSitesUseCases.RemoveTopSiteUseCase =
            mockk(relaxed = true)
        val topSite: TopSite = mockk()
        every { topSite.url } returns selectedTab.content.url
        coEvery { pinnedSiteStorage.getPinnedSites() } returns listOf(topSite)
        every { topSitesUseCase.removeTopSites } returns removePinnedSiteUseCase
        every {
            snackbarParent.context.getString(R.string.snackbar_top_site_removed)
        } returns snackbarMessage

        val controller = createController(scope = this, store = browserStore)
        assertNull(Events.browserMenuAction.testGetValue())

        controller.handleToolbarItemInteraction(item)

        assertNotNull(Events.browserMenuAction.testGetValue())
        val snapshot = Events.browserMenuAction.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("remove_from_top_sites", snapshot.single().extra?.getValue("item"))

        verify { snackbar.setText(snackbarMessage) }
        verify { removePinnedSiteUseCase.invoke(topSite) }
    }

    @Test
    fun `WHEN addon extensions menu item is pressed THEN navigate to addons manager`() = runTest {
        val item = ToolbarMenu.Item.AddonsManager

        val controller = createController(scope = this, store = browserStore)
        assertNull(Events.browserMenuAction.testGetValue())

        controller.handleToolbarItemInteraction(item)

        assertNotNull(Events.browserMenuAction.testGetValue())
        val snapshot = Events.browserMenuAction.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("addons_manager", snapshot.single().extra?.getValue("item"))
    }

    @Test
    fun `WHEN Add To Home Screen menu item is pressed THEN add site`() = runTest {
        val item = ToolbarMenu.Item.AddToHomeScreen

        val controller = createController(scope = this, store = browserStore)
        assertNull(Events.browserMenuAction.testGetValue())

        controller.handleToolbarItemInteraction(item)

        assertNotNull(Events.browserMenuAction.testGetValue())
        val snapshot = Events.browserMenuAction.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("add_to_homescreen", snapshot.single().extra?.getValue("item"))
    }

    @Test
    fun `IF reader mode is inactive WHEN share menu item is pressed THEN navigate to share screen`() = runTest {
        val item = ToolbarMenu.Item.Share
        val title = "Mozilla"
        val url = "https://mozilla.org"
        val regularTab = createTab(
            url = url,
            readerState = ReaderState(active = false, activeUrl = "https://1234.org"),
            title = title,
        )
        browserStore = BrowserStore(BrowserState(tabs = listOf(regularTab), selectedTabId = regularTab.id))
        val controller = createController(scope = this, store = browserStore)
        assertNull(Events.browserMenuAction.testGetValue())

        controller.handleToolbarItemInteraction(item)

        assertNotNull(Events.browserMenuAction.testGetValue())
        val snapshot = Events.browserMenuAction.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("share", snapshot.single().extra?.getValue("item"))

        verify {
            navController.navigate(
                directionsEq(
                    NavGraphDirections.actionGlobalShareFragment(
                        sessionId = browserStore.state.selectedTabId,
                        data = arrayOf(ShareData(url = "https://mozilla.org", title = "Mozilla")),
                        showPage = true,
                    ),
                ),
            )
        }
    }

    @Test
    fun `IF reader mode is active WHEN share menu item is pressed THEN navigate to share screen`() = runTest {
        val item = ToolbarMenu.Item.Share
        val title = "Mozilla"
        val readerUrl = "moz-extension://1234"
        val readerTab = createTab(
            url = readerUrl,
            readerState = ReaderState(active = true, activeUrl = "https://mozilla.org"),
            title = title,
        )
        browserStore = BrowserStore(BrowserState(tabs = listOf(readerTab), selectedTabId = readerTab.id))
        val controller = createController(scope = this, store = browserStore)
        assertNull(Events.browserMenuAction.testGetValue())

        controller.handleToolbarItemInteraction(item)

        assertNotNull(Events.browserMenuAction.testGetValue())
        val snapshot = Events.browserMenuAction.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("share", snapshot.single().extra?.getValue("item"))

        verify {
            navController.navigate(
                directionsEq(
                    NavGraphDirections.actionGlobalShareFragment(
                        sessionId = browserStore.state.selectedTabId,
                        data = arrayOf(ShareData(url = "https://mozilla.org", title = "Mozilla")),
                        showPage = true,
                    ),
                ),
            )
        }
    }

    @Test
    fun `WHEN Find In Page menu item is pressed THEN launch finder`() = runTest {
        val item = ToolbarMenu.Item.FindInPage

        var launcherInvoked = false
        val controller = createController(
            scope = this,
            store = browserStore,
            findInPageLauncher = {
                launcherInvoked = true
            },
        )
        controller.handleToolbarItemInteraction(item)

        assertTrue(launcherInvoked)
    }

    @Test
    fun `IF one or more collection exists WHEN Save To Collection menu item is pressed THEN navigate to save collection page`() = runTest {
        val item = ToolbarMenu.Item.SaveToCollection
        val cachedTabCollections: List<TabCollection> = mockk(relaxed = true)
        every { tabCollectionStorage.cachedTabCollections } returns cachedTabCollections

        val controller = createController(scope = this, store = browserStore)
        assertNull(Events.browserMenuAction.testGetValue())

        controller.handleToolbarItemInteraction(item)

        assertNotNull(Events.browserMenuAction.testGetValue())
        val snapshot = Events.browserMenuAction.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("save_to_collection", snapshot.single().extra?.getValue("item"))

        assertNotNull(Collections.saveButton.testGetValue())
        val recordedEvents = Collections.saveButton.testGetValue()!!
        assertEquals(1, recordedEvents.size)
        val eventExtra = recordedEvents.single().extra
        assertNotNull(eventExtra)
        assertTrue(eventExtra!!.containsKey("from_screen"))
        assertEquals(
            DefaultBrowserToolbarMenuController.TELEMETRY_BROWSER_IDENTIFIER,
            eventExtra["from_screen"],
        )

        val directions = BrowserFragmentDirections.actionGlobalCollectionCreationFragment(
            saveCollectionStep = SaveCollectionStep.SelectCollection,
            tabIds = arrayOf(selectedTab.id),
            selectedTabIds = arrayOf(selectedTab.id),
        )
        verify { navController.navigate(directionsEq(directions), null) }
    }

    @Test
    fun `IF no collection exists WHEN Save To Collection menu item is pressed THEN navigate to create collection page`() = runTest {
        val item = ToolbarMenu.Item.SaveToCollection
        val cachedTabCollectionsEmpty: List<TabCollection> = emptyList()
        every { tabCollectionStorage.cachedTabCollections } returns cachedTabCollectionsEmpty

        val controller = createController(scope = this, store = browserStore)
        assertNull(Events.browserMenuAction.testGetValue())

        controller.handleToolbarItemInteraction(item)

        assertNotNull(Events.browserMenuAction.testGetValue())
        val snapshot = Events.browserMenuAction.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("save_to_collection", snapshot.single().extra?.getValue("item"))

        assertNotNull(Collections.saveButton.testGetValue())
        val recordedEvents = Collections.saveButton.testGetValue()!!
        assertEquals(1, recordedEvents.size)
        val eventExtra = recordedEvents.single().extra
        assertNotNull(eventExtra)
        assertTrue(eventExtra!!.containsKey("from_screen"))
        assertEquals(
            DefaultBrowserToolbarMenuController.TELEMETRY_BROWSER_IDENTIFIER,
            eventExtra["from_screen"],
        )
        val directions = BrowserFragmentDirections.actionGlobalCollectionCreationFragment(
            saveCollectionStep = SaveCollectionStep.NameCollection,
            tabIds = arrayOf(selectedTab.id),
            selectedTabIds = arrayOf(selectedTab.id),
        )
        verify { navController.navigate(directionsEq(directions), null) }
    }

    @Test
    fun `WHEN print menu item is pressed THEN request print`() = runTest {
        val item = ToolbarMenu.Item.PrintContent

        val controller = createController(scope = this, store = browserStore)
        assertNull(Events.browserMenuAction.testGetValue())

        controller.handleToolbarItemInteraction(item)

        assertNotNull(Events.browserMenuAction.testGetValue())
        val snapshot = Events.browserMenuAction.testGetValue()!!
        assertEquals(1, snapshot.size)
        assertEquals("print_content", snapshot.single().extra?.getValue("item"))
    }

    @Test
    fun `WHEN New Tab menu item is pressed THEN navigate to a new tab home`() = runTest {
        val item = ToolbarMenu.Item.NewTab

        val controller = createController(scope = this, store = browserStore)

        controller.handleToolbarItemInteraction(item)

        verify {
            navController.navigate(
                directionsEq(
                    NavGraphDirections.actionGlobalHome(
                        focusOnAddressBar = true,
                    ),
                ),
            )
        }
    }

    @Test
    fun `GIVEN account exists and the user is signed in WHEN sign in to sync menu item is pressed THEN navigate to account settings`() = runTest {
        val item = ToolbarMenu.Item.SyncAccount(AccountState.AUTHENTICATED)
        val accountSettingsDirections = BrowserFragmentDirections.actionGlobalAccountSettingsFragment()
        val controller = createController(scope = this, store = browserStore)

        controller.handleToolbarItemInteraction(item)

        verify { navController.navigate(accountSettingsDirections, null) }
    }

    @Test
    fun `GIVEN account exists and the user is not signed in WHEN sign in to sync menu item is pressed THEN navigate to account problem fragment`() = runTest {
        val item = ToolbarMenu.Item.SyncAccount(AccountState.NEEDS_REAUTHENTICATION)
        val accountProblemDirections = BrowserFragmentDirections.actionGlobalAccountProblemFragment(
            entrypoint = FenixFxAEntryPoint.BrowserToolbar,
        )
        val controller = createController(scope = this, store = browserStore)

        controller.handleToolbarItemInteraction(item)

        verify { navController.navigate(accountProblemDirections, null) }
    }

    @Test
    fun `GIVEN account doesn't exist WHEN sign in to sync menu item is pressed THEN navigate to sign in`() = runTest {
        val item = ToolbarMenu.Item.SyncAccount(AccountState.NO_ACCOUNT)
        val turnOnSyncDirections = BrowserFragmentDirections.actionGlobalTurnOnSync(
            entrypoint = FenixFxAEntryPoint.BrowserToolbar,
        )
        val controller = createController(scope = this, store = browserStore)

        controller.handleToolbarItemInteraction(item)

        verify { navController.navigate(turnOnSyncDirections, null) }
    }

    @Test
    fun `WHEN the Translations menu item is pressed THEN navigate to translations flow`() =
        runTest {
            val item = ToolbarMenu.Item.Translate

            val controller = createController(scope = this, store = browserStore)

            controller.handleToolbarItemInteraction(item)

            verify {
                navController.navigate(
                    directionsEq(
                        BrowserFragmentDirections.actionBrowserFragmentToTranslationsDialogFragment(
                            sessionId = selectedTab.id,
                        ),
                    ),
                )
            }
        }

    private fun createController(
        scope: CoroutineScope,
        store: BrowserStore,
        activity: HomeActivity = this.activity,
        customTabSessionId: String? = null,
        findInPageLauncher: () -> Unit = { },
        bookmarkTapped: (String, String) -> Unit = { _, _ -> },
    ) = DefaultBrowserToolbarMenuController(
        store = store,
        activity = activity,
        navController = navController,
        settings = settings,
        findInPageLauncher = findInPageLauncher,
        browserAnimator = browserAnimator,
        customTabSessionId = customTabSessionId,
        openInFenixIntent = openInFenixIntent,
        scope = scope,
        snackbarParent = snackbarParent,
        tabCollectionStorage = tabCollectionStorage,
        bookmarkTapped = bookmarkTapped,
        readerModeController = readerModeController,
        sessionFeature = sessionFeatureWrapper,
        topSitesStorage = topSitesStorage,
        pinnedSiteStorage = pinnedSiteStorage,
        browserStore = browserStore,
    ).apply {
        ioScope = scope
    }
}
