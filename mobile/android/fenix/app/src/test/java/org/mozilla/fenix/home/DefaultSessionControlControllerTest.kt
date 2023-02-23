/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import androidx.navigation.NavController
import androidx.navigation.NavDirections
import androidx.navigation.NavOptions
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.mockkObject
import io.mockk.mockkStatic
import io.mockk.slot
import io.mockk.spyk
import io.mockk.unmockkObject
import io.mockk.unmockkStatic
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.state.action.SearchAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.search.RegionState
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ReaderState
import mozilla.components.browser.state.state.SearchState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.state.recover.RecoverableTab
import mozilla.components.browser.state.state.recover.TabState
import mozilla.components.browser.state.state.selectedOrDefaultSearchEngine
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.GleanMetrics.Collections
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.GleanMetrics.HomeScreen
import org.mozilla.fenix.GleanMetrics.Pings
import org.mozilla.fenix.GleanMetrics.RecentBookmarks
import org.mozilla.fenix.GleanMetrics.RecentTabs
import org.mozilla.fenix.GleanMetrics.TopSites
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.Analytics
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.gleanplumb.Message
import org.mozilla.fenix.gleanplumb.MessageController
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.recentbookmarks.RecentBookmark
import org.mozilla.fenix.home.recenttabs.RecentTab
import org.mozilla.fenix.home.sessioncontrol.DefaultSessionControlController
import org.mozilla.fenix.onboarding.WallpaperOnboardingDialogFragment.Companion.THUMBNAILS_SELECTION_COUNT
import org.mozilla.fenix.search.toolbar.SearchSelectorMenu
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.utils.Settings
import org.mozilla.fenix.wallpapers.Wallpaper
import org.mozilla.fenix.wallpapers.WallpaperState
import mozilla.components.feature.tab.collections.Tab as ComponentTab

@RunWith(FenixRobolectricTestRunner::class) // For gleanTestRule
@OptIn(ExperimentalCoroutinesApi::class)
class DefaultSessionControlControllerTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    private val activity: HomeActivity = mockk(relaxed = true)
    private val appStore: AppStore = mockk(relaxed = true)
    private val navController: NavController = mockk(relaxed = true)
    private val messageController: MessageController = mockk(relaxed = true)
    private val engine: Engine = mockk(relaxed = true)
    private val tabCollectionStorage: TabCollectionStorage = mockk(relaxed = true)
    private val tabsUseCases: TabsUseCases = mockk(relaxed = true)
    private val reloadUrlUseCase: SessionUseCases = mockk(relaxed = true)
    private val selectTabUseCase: TabsUseCases = mockk(relaxed = true)
    private val settings: Settings = mockk(relaxed = true)
    private val analytics: Analytics = mockk(relaxed = true)
    private val scope = coroutinesTestRule.scope
    private val searchEngine = SearchEngine(
        id = "test",
        name = "Test Engine",
        icon = mockk(relaxed = true),
        type = SearchEngine.Type.BUNDLED,
        resultUrls = listOf("https://example.org/?q={searchTerms}"),
    )

    private val googleSearchEngine = SearchEngine(
        id = "googleTest",
        name = "Google Test Engine",
        icon = mockk(relaxed = true),
        type = SearchEngine.Type.BUNDLED,
        resultUrls = listOf("https://www.google.com/?q={searchTerms}"),
        suggestUrl = "https://www.google.com/",
    )

    private val duckDuckGoSearchEngine = SearchEngine(
        id = "ddgTest",
        name = "DuckDuckGo Test Engine",
        icon = mockk(relaxed = true),
        type = SearchEngine.Type.BUNDLED,
        resultUrls = listOf("https://duckduckgo.com/?q=%7BsearchTerms%7D&t=fpas"),
        suggestUrl = "https://ac.duckduckgo.com/ac/?q=%7BsearchTerms%7D&type=list",
    )

    private lateinit var store: BrowserStore
    private val appState: AppState = mockk(relaxed = true)

    @Before
    fun setup() {
        store = BrowserStore(
            BrowserState(
                search = SearchState(
                    regionSearchEngines = listOf(searchEngine),
                ),
            ),
        )

        every { appStore.state } returns AppState(
            collections = emptyList(),
            expandedCollections = emptySet(),
            mode = Mode.Normal,
            topSites = emptyList(),
            showCollectionPlaceholder = true,
            recentTabs = emptyList(),
            recentBookmarks = emptyList(),
        )

        every { navController.currentDestination } returns mockk {
            every { id } returns R.id.homeFragment
        }
        every { activity.components.settings } returns settings
        every { activity.settings() } returns settings
        every { activity.components.analytics } returns analytics
    }

    @Test
    fun handleCollectionAddTabTapped() {
        val collection = mockk<TabCollection> {
            every { id } returns 12L
        }
        createController().handleCollectionAddTabTapped(collection)

        assertNotNull(Collections.addTabButton.testGetValue())
        val recordedEvents = Collections.addTabButton.testGetValue()!!
        assertEquals(1, recordedEvents.size)
        assertEquals(null, recordedEvents.single().extra)

        verify {
            navController.navigate(
                match<NavDirections> {
                    it.actionId == R.id.action_global_collectionCreationFragment
                },
                null,
            )
        }
    }

    @Test
    fun handleCustomizeHomeTapped() {
        assertNull(HomeScreen.customizeHomeClicked.testGetValue())

        createController().handleCustomizeHomeTapped()

        assertNotNull(HomeScreen.customizeHomeClicked.testGetValue())
        verify {
            navController.navigate(
                match<NavDirections> {
                    it.actionId == R.id.action_global_homeSettingsFragment
                },
                null,
            )
        }
    }

    @Test
    fun `handleCollectionOpenTabClicked onFailure`() {
        val tab = mockk<ComponentTab> {
            every { url } returns "https://mozilla.org"
            every { restore(activity, engine, restoreSessionId = false) } returns null
        }
        createController().handleCollectionOpenTabClicked(tab)

        assertNotNull(Collections.tabRestored.testGetValue())
        val recordedEvents = Collections.tabRestored.testGetValue()!!
        assertEquals(1, recordedEvents.size)
        assertEquals(null, recordedEvents.single().extra)

        verify {
            activity.openToBrowserAndLoad(
                searchTermOrURL = "https://mozilla.org",
                newTab = true,
                from = BrowserDirection.FromHome,
            )
        }
    }

    @Test
    fun `handleCollectionOpenTabClicked with existing selected tab`() {
        val recoverableTab = RecoverableTab(
            engineSessionState = null,
            state = TabState(
                id = "test",
                parentId = null,
                url = "https://www.mozilla.org",
                title = "Mozilla",
                contextId = null,
                readerState = ReaderState(),
                lastAccess = 0,
                private = false,
            ),
        )

        val tab = mockk<ComponentTab> {
            every { restore(activity, engine, restoreSessionId = false) } returns recoverableTab
        }

        val restoredTab = createTab(id = recoverableTab.state.id, url = recoverableTab.state.url)
        val otherTab = createTab(id = "otherTab", url = "https://mozilla.org")
        store.dispatch(TabListAction.AddTabAction(otherTab)).joinBlocking()
        store.dispatch(TabListAction.SelectTabAction(otherTab.id)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(restoredTab)).joinBlocking()

        createController().handleCollectionOpenTabClicked(tab)

        assertNotNull(Collections.tabRestored.testGetValue())
        val recordedEvents = Collections.tabRestored.testGetValue()!!
        assertEquals(1, recordedEvents.size)
        assertEquals(null, recordedEvents.single().extra)

        verify { activity.openToBrowser(BrowserDirection.FromHome) }
        verify { selectTabUseCase.selectTab.invoke(restoredTab.id) }
        verify { reloadUrlUseCase.reload.invoke(restoredTab.id) }
    }

    @Test
    fun `handleCollectionOpenTabClicked without existing selected tab`() {
        val recoverableTab = RecoverableTab(
            engineSessionState = null,
            state = TabState(
                id = "test",
                parentId = null,
                url = "https://www.mozilla.org",
                title = "Mozilla",
                contextId = null,
                readerState = ReaderState(),
                lastAccess = 0,
                private = false,
            ),
        )

        val tab = mockk<ComponentTab> {
            every { restore(activity, engine, restoreSessionId = false) } returns recoverableTab
        }

        val restoredTab = createTab(id = recoverableTab.state.id, url = recoverableTab.state.url)
        store.dispatch(TabListAction.AddTabAction(restoredTab)).joinBlocking()

        createController().handleCollectionOpenTabClicked(tab)

        assertNotNull(Collections.tabRestored.testGetValue())
        val recordedEvents = Collections.tabRestored.testGetValue()!!
        assertEquals(1, recordedEvents.size)
        assertEquals(null, recordedEvents.single().extra)

        verify { activity.openToBrowser(BrowserDirection.FromHome) }
        verify { selectTabUseCase.selectTab.invoke(restoredTab.id) }
        verify { reloadUrlUseCase.reload.invoke(restoredTab.id) }
    }

    @Test
    fun handleCollectionOpenTabsTapped() {
        val collection = mockk<TabCollection> {
            every { tabs } returns emptyList()
        }
        createController().handleCollectionOpenTabsTapped(collection)

        assertNotNull(Collections.allTabsRestored.testGetValue())
        val recordedEvents = Collections.allTabsRestored.testGetValue()!!
        assertEquals(1, recordedEvents.size)
        assertEquals(null, recordedEvents.single().extra)
    }

    @Test
    fun `handleCollectionRemoveTab one tab`() {
        val expectedCollection = mockk<TabCollection> {
            every { tabs } returns listOf(mockk())
            every { title } returns "Collection"
        }
        val tab = mockk<ComponentTab>()
        every {
            activity.resources.getString(
                R.string.delete_tab_and_collection_dialog_title,
                "Collection",
            )
        } returns "Delete Collection?"
        every {
            activity.resources.getString(R.string.delete_tab_and_collection_dialog_message)
        } returns "Deleting this tab will delete everything."

        var actualCollection: TabCollection? = null

        createController(
            removeCollectionWithUndo = { collection ->
                actualCollection = collection
            },
        ).handleCollectionRemoveTab(expectedCollection, tab, false)

        assertNotNull(Collections.tabRemoved.testGetValue())
        val recordedEvents = Collections.tabRemoved.testGetValue()!!
        assertEquals(1, recordedEvents.size)
        assertEquals(null, recordedEvents.single().extra)

        assertEquals(expectedCollection, actualCollection)
    }

    @Test
    fun `handleCollectionRemoveTab multiple tabs`() {
        val collection: TabCollection = mockk(relaxed = true)
        val tab: ComponentTab = mockk(relaxed = true)
        createController().handleCollectionRemoveTab(collection, tab, false)

        assertNotNull(Collections.tabRemoved.testGetValue())
        val recordedEvents = Collections.tabRemoved.testGetValue()!!
        assertEquals(1, recordedEvents.size)
        assertEquals(null, recordedEvents.single().extra)
    }

    @Test
    fun handleCollectionShareTabsClicked() {
        val collection = mockk<TabCollection> {
            every { tabs } returns emptyList()
            every { title } returns ""
        }
        createController().handleCollectionShareTabsClicked(collection)

        assertNotNull(Collections.shared.testGetValue())
        val recordedEvents = Collections.shared.testGetValue()!!
        assertEquals(1, recordedEvents.size)
        assertEquals(null, recordedEvents.single().extra)

        verify {
            navController.navigate(
                match<NavDirections> { it.actionId == R.id.action_global_shareFragment },
                null,
            )
        }
    }

    @Test
    fun handleDeleteCollectionTapped() {
        val expectedCollection = mockk<TabCollection> {
            every { title } returns "Collection"
        }
        every {
            activity.resources.getString(R.string.tab_collection_dialog_message, "Collection")
        } returns "Are you sure you want to delete Collection?"

        var actualCollection: TabCollection? = null

        createController(
            removeCollectionWithUndo = { collection ->
                actualCollection = collection
            },
        ).handleDeleteCollectionTapped(expectedCollection)

        assertEquals(expectedCollection, actualCollection)
        assertNotNull(Collections.removed.testGetValue())
        val recordedEvents = Collections.removed.testGetValue()!!
        assertEquals(1, recordedEvents.size)
        assertEquals(null, recordedEvents.single().extra)
    }

    @Test
    fun handlePrivateBrowsingLearnMoreClicked() {
        createController().handlePrivateBrowsingLearnMoreClicked()
        verify {
            activity.openToBrowserAndLoad(
                searchTermOrURL = SupportUtils.getGenericSumoURLForTopic
                (SupportUtils.SumoTopic.PRIVATE_BROWSING_MYTHS),
                newTab = true,
                from = BrowserDirection.FromHome,
            )
        }
    }

    @Test
    fun handleRenameCollectionTapped() {
        val collection = mockk<TabCollection> {
            every { id } returns 3L
        }
        createController().handleRenameCollectionTapped(collection)

        assertNotNull(Collections.renameButton.testGetValue())
        val recordedEvents = Collections.renameButton.testGetValue()!!
        assertEquals(1, recordedEvents.size)
        assertEquals(null, recordedEvents.single().extra)

        verify {
            navController.navigate(
                match<NavDirections> { it.actionId == R.id.action_global_collectionCreationFragment },
                null,
            )
        }
    }

    @Test
    fun handleSelectDefaultTopSite() {
        val topSite = TopSite.Default(
            id = 1L,
            title = "Mozilla",
            url = "mozilla.org",
            createdAt = 0,
        )
        val controller = spyk(createController())

        every { controller.getAvailableSearchEngines() } returns listOf(searchEngine)

        controller.handleSelectTopSite(topSite, position = 0)

        assertNotNull(TopSites.openInNewTab.testGetValue())
        assertEquals(1, TopSites.openInNewTab.testGetValue()!!.size)
        assertNull(TopSites.openInNewTab.testGetValue()!!.single().extra)

        assertNotNull(TopSites.openDefault.testGetValue())
        assertEquals(1, TopSites.openDefault.testGetValue()!!.size)
        assertNull(TopSites.openDefault.testGetValue()!!.single().extra)

        verify {
            tabsUseCases.addTab.invoke(
                url = topSite.url,
                selectTab = true,
                startLoading = true,
            )
        }
        verify { activity.openToBrowser(BrowserDirection.FromHome) }
    }

    @Test
    fun handleSelectNonDefaultTopSite() {
        val topSite = TopSite.Frecent(
            id = 1L,
            title = "Mozilla",
            url = "mozilla.org",
            createdAt = 0,
        )
        val controller = spyk(createController())

        every { controller.getAvailableSearchEngines() } returns listOf(searchEngine)

        controller.handleSelectTopSite(topSite, position = 0)

        assertNotNull(TopSites.openInNewTab.testGetValue())
        assertEquals(1, TopSites.openInNewTab.testGetValue()!!.size)
        assertNull(TopSites.openInNewTab.testGetValue()!!.single().extra)

        verify {
            tabsUseCases.addTab.invoke(
                url = topSite.url,
                selectTab = true,
                startLoading = true,
            )
        }
        verify { activity.openToBrowser(BrowserDirection.FromHome) }
    }

    @Test
    fun handleSelectGoogleDefaultTopSiteUS() {
        val topSite = TopSite.Default(
            id = 1L,
            title = "Google",
            url = SupportUtils.GOOGLE_URL,
            createdAt = 0,
        )
        val controller = spyk(createController())

        every { controller.getAvailableSearchEngines() } returns listOf(searchEngine)

        store.dispatch(SearchAction.SetRegionAction(RegionState("US", "US"))).joinBlocking()

        controller.handleSelectTopSite(topSite, position = 0)

        assertNotNull(TopSites.openInNewTab.testGetValue())
        assertEquals(1, TopSites.openInNewTab.testGetValue()!!.size)
        assertNull(TopSites.openInNewTab.testGetValue()!!.single().extra)

        assertNotNull(TopSites.openDefault.testGetValue())
        assertEquals(1, TopSites.openDefault.testGetValue()!!.size)
        assertNull(TopSites.openDefault.testGetValue()!!.single().extra)

        assertNotNull(TopSites.openGoogleSearchAttribution.testGetValue())
        assertEquals(1, TopSites.openGoogleSearchAttribution.testGetValue()!!.size)
        assertNull(TopSites.openGoogleSearchAttribution.testGetValue()!!.single().extra)

        verify {
            tabsUseCases.addTab.invoke(
                url = SupportUtils.GOOGLE_US_URL,
                selectTab = true,
                startLoading = true,
            )
        }
        verify { activity.openToBrowser(BrowserDirection.FromHome) }
    }

    @Test
    fun handleSelectGoogleDefaultTopSiteXX() {
        val topSite = TopSite.Default(
            id = 1L,
            title = "Google",
            url = SupportUtils.GOOGLE_URL,
            createdAt = 0,
        )
        val controller = spyk(createController())

        every { controller.getAvailableSearchEngines() } returns listOf(searchEngine)

        store.dispatch(SearchAction.SetRegionAction(RegionState("DE", "FR"))).joinBlocking()

        controller.handleSelectTopSite(topSite, position = 0)

        assertNotNull(TopSites.openInNewTab.testGetValue())
        assertEquals(1, TopSites.openInNewTab.testGetValue()!!.size)
        assertNull(TopSites.openInNewTab.testGetValue()!!.single().extra)

        assertNotNull(TopSites.openDefault.testGetValue())
        assertEquals(1, TopSites.openDefault.testGetValue()!!.size)
        assertNull(TopSites.openDefault.testGetValue()!!.single().extra)

        assertNotNull(TopSites.openGoogleSearchAttribution.testGetValue())
        assertEquals(1, TopSites.openGoogleSearchAttribution.testGetValue()!!.size)
        assertNull(TopSites.openGoogleSearchAttribution.testGetValue()!!.single().extra)

        verify {
            tabsUseCases.addTab.invoke(
                SupportUtils.GOOGLE_XX_URL,
                selectTab = true,
                startLoading = true,
            )
        }
        verify { activity.openToBrowser(BrowserDirection.FromHome) }
    }

    @Test
    fun handleSelectGoogleDefaultTopSite_EventPerformedSearchTopSite() {
        assertNull(Events.performedSearch.testGetValue())

        val topSite = TopSite.Default(
            id = 1L,
            title = "Google",
            url = SupportUtils.GOOGLE_URL,
            createdAt = 0,
        )
        val controller = spyk(createController())

        every { controller.getAvailableSearchEngines() } returns listOf(googleSearchEngine)

        try {
            mockkStatic("mozilla.components.browser.state.state.SearchStateKt")

            every { any<SearchState>().selectedOrDefaultSearchEngine } returns googleSearchEngine

            controller.handleSelectTopSite(topSite, position = 0)

            assertNotNull(Events.performedSearch.testGetValue())

            assertNotNull(TopSites.openDefault.testGetValue())
            assertEquals(1, TopSites.openDefault.testGetValue()!!.size)
            assertNull(TopSites.openDefault.testGetValue()!!.single().extra)

            assertNotNull(TopSites.openGoogleSearchAttribution.testGetValue())
            assertEquals(1, TopSites.openGoogleSearchAttribution.testGetValue()!!.size)
            assertNull(TopSites.openGoogleSearchAttribution.testGetValue()!!.single().extra)
        } finally {
            unmockkStatic("mozilla.components.browser.state.state.SearchStateKt")
        }
    }

    @Test
    fun handleSelectDuckDuckGoTopSite_EventPerformedSearchTopSite() {
        assertNull(Events.performedSearch.testGetValue())

        val topSite = TopSite.Pinned(
            id = 1L,
            title = "DuckDuckGo",
            url = "https://duckduckgo.com",
            createdAt = 0,
        )
        val controller = spyk(createController())

        every { controller.getAvailableSearchEngines() } returns listOf(googleSearchEngine, duckDuckGoSearchEngine)

        try {
            mockkStatic("mozilla.components.browser.state.state.SearchStateKt")

            every { any<SearchState>().selectedOrDefaultSearchEngine } returns googleSearchEngine

            controller.handleSelectTopSite(topSite, position = 0)

            assertNotNull(Events.performedSearch.testGetValue())
        } finally {
            unmockkStatic("mozilla.components.browser.state.state.SearchStateKt")
        }
    }

    @Test
    fun handleSelectGooglePinnedTopSiteUS() {
        val topSite = TopSite.Pinned(
            id = 1L,
            title = "Google",
            url = SupportUtils.GOOGLE_URL,
            createdAt = 0,
        )
        val controller = spyk(createController())

        every { controller.getAvailableSearchEngines() } returns listOf(searchEngine)

        store.dispatch(SearchAction.SetRegionAction(RegionState("US", "US"))).joinBlocking()

        controller.handleSelectTopSite(topSite, position = 0)

        assertNotNull(TopSites.openInNewTab.testGetValue())
        assertEquals(1, TopSites.openInNewTab.testGetValue()!!.size)
        assertNull(TopSites.openInNewTab.testGetValue()!!.single().extra)

        assertNotNull(TopSites.openPinned.testGetValue())
        assertEquals(1, TopSites.openPinned.testGetValue()!!.size)
        assertNull(TopSites.openPinned.testGetValue()!!.single().extra)

        assertNotNull(TopSites.openGoogleSearchAttribution.testGetValue())
        assertEquals(1, TopSites.openGoogleSearchAttribution.testGetValue()!!.size)
        assertNull(TopSites.openGoogleSearchAttribution.testGetValue()!!.single().extra)

        verify {
            tabsUseCases.addTab.invoke(
                SupportUtils.GOOGLE_US_URL,
                selectTab = true,
                startLoading = true,
            )
        }
        verify { activity.openToBrowser(BrowserDirection.FromHome) }
    }

    @Test
    fun handleSelectGooglePinnedTopSiteXX() {
        val topSite = TopSite.Pinned(
            id = 1L,
            title = "Google",
            url = SupportUtils.GOOGLE_URL,
            createdAt = 0,
        )
        val controller = spyk(createController())

        every { controller.getAvailableSearchEngines() } returns listOf(searchEngine)

        store.dispatch(SearchAction.SetRegionAction(RegionState("DE", "FR"))).joinBlocking()

        controller.handleSelectTopSite(topSite, position = 0)

        assertNotNull(TopSites.openInNewTab.testGetValue())
        assertEquals(1, TopSites.openInNewTab.testGetValue()!!.size)
        assertNull(TopSites.openInNewTab.testGetValue()!!.single().extra)

        assertNotNull(TopSites.openPinned.testGetValue())
        assertEquals(1, TopSites.openPinned.testGetValue()!!.size)
        assertNull(TopSites.openPinned.testGetValue()!!.single().extra)

        assertNotNull(TopSites.openGoogleSearchAttribution.testGetValue())
        assertEquals(1, TopSites.openGoogleSearchAttribution.testGetValue()!!.size)
        assertNull(TopSites.openGoogleSearchAttribution.testGetValue()!!.single().extra)

        verify {
            tabsUseCases.addTab.invoke(
                SupportUtils.GOOGLE_XX_URL,
                selectTab = true,
                startLoading = true,
            )
        }
        verify { activity.openToBrowser(BrowserDirection.FromHome) }
    }

    @Test
    fun handleSelectGoogleFrecentTopSiteUS() {
        val topSite = TopSite.Frecent(
            id = 1L,
            title = "Google",
            url = SupportUtils.GOOGLE_URL,
            createdAt = 0,
        )
        val controller = spyk(createController())

        every { controller.getAvailableSearchEngines() } returns listOf(searchEngine)

        store.dispatch(SearchAction.SetRegionAction(RegionState("US", "US"))).joinBlocking()

        controller.handleSelectTopSite(topSite, position = 0)

        assertNotNull(TopSites.openInNewTab.testGetValue())
        assertEquals(1, TopSites.openInNewTab.testGetValue()!!.size)
        assertNull(TopSites.openInNewTab.testGetValue()!!.single().extra)

        assertNotNull(TopSites.openFrecency.testGetValue())
        assertEquals(1, TopSites.openFrecency.testGetValue()!!.size)
        assertNull(TopSites.openFrecency.testGetValue()!!.single().extra)

        assertNotNull(TopSites.openGoogleSearchAttribution.testGetValue())
        assertEquals(1, TopSites.openGoogleSearchAttribution.testGetValue()!!.size)
        assertNull(TopSites.openGoogleSearchAttribution.testGetValue()!!.single().extra)

        verify {
            tabsUseCases.addTab.invoke(
                SupportUtils.GOOGLE_US_URL,
                selectTab = true,
                startLoading = true,
            )
        }
        verify { activity.openToBrowser(BrowserDirection.FromHome) }
    }

    @Test
    fun handleSelectGoogleFrecentTopSiteXX() {
        val topSite = TopSite.Frecent(
            id = 1L,
            title = "Google",
            url = SupportUtils.GOOGLE_URL,
            createdAt = 0,
        )
        val controller = spyk(createController())

        every { controller.getAvailableSearchEngines() } returns listOf(searchEngine)

        store.dispatch(SearchAction.SetRegionAction(RegionState("DE", "FR"))).joinBlocking()

        controller.handleSelectTopSite(topSite, position = 0)

        assertNotNull(TopSites.openInNewTab.testGetValue())
        assertEquals(1, TopSites.openInNewTab.testGetValue()!!.size)
        assertNull(TopSites.openInNewTab.testGetValue()!!.single().extra)

        assertNotNull(TopSites.openFrecency.testGetValue())
        assertEquals(1, TopSites.openFrecency.testGetValue()!!.size)
        assertNull(TopSites.openFrecency.testGetValue()!!.single().extra)

        assertNotNull(TopSites.openGoogleSearchAttribution.testGetValue())
        assertEquals(1, TopSites.openGoogleSearchAttribution.testGetValue()!!.size)
        assertNull(TopSites.openGoogleSearchAttribution.testGetValue()!!.single().extra)

        verify {
            tabsUseCases.addTab.invoke(
                SupportUtils.GOOGLE_XX_URL,
                selectTab = true,
                startLoading = true,
            )
        }
        verify { activity.openToBrowser(BrowserDirection.FromHome) }
    }

    @Test
    fun handleSelectProvidedTopSite() {
        val topSite = TopSite.Provided(
            id = 1L,
            title = "Mozilla",
            url = "mozilla.org",
            clickUrl = "",
            imageUrl = "",
            impressionUrl = "",
            createdAt = 0,
        )
        val position = 0
        val controller = spyk(createController())

        every { controller.getAvailableSearchEngines() } returns listOf(searchEngine)

        controller.handleSelectTopSite(topSite, position)

        assertNotNull(TopSites.openInNewTab.testGetValue())
        assertEquals(1, TopSites.openInNewTab.testGetValue()!!.size)
        assertNull(TopSites.openInNewTab.testGetValue()!!.single().extra)

        assertNotNull(TopSites.openContileTopSite.testGetValue())
        assertEquals(1, TopSites.openContileTopSite.testGetValue()!!.size)
        assertNull(TopSites.openContileTopSite.testGetValue()!!.single().extra)

        verify {
            tabsUseCases.addTab.invoke(
                url = topSite.url,
                selectTab = true,
                startLoading = true,
            )
        }
        verify { controller.submitTopSitesImpressionPing(topSite, position) }
        verify { activity.openToBrowser(BrowserDirection.FromHome) }
    }

    @Test
    fun `GIVEN a provided top site WHEN the provided top site is clicked THEN submit a top site impression ping`() {
        val controller = spyk(createController())
        val topSite = TopSite.Provided(
            id = 3,
            title = "Mozilla",
            url = "https://mozilla.com",
            clickUrl = "https://mozilla.com/click",
            imageUrl = "https://test.com/image2.jpg",
            impressionUrl = "https://example.com",
            createdAt = 3,
        )
        val position = 0
        assertNull(TopSites.contileImpression.testGetValue())

        var topSiteImpressionPinged = false
        Pings.topsitesImpression.testBeforeNextSubmit {
            assertNotNull(TopSites.contileTileId.testGetValue())
            assertEquals(3L, TopSites.contileTileId.testGetValue())

            assertNotNull(TopSites.contileAdvertiser.testGetValue())
            assertEquals("mozilla", TopSites.contileAdvertiser.testGetValue())

            assertNotNull(TopSites.contileReportingUrl.testGetValue())
            assertEquals(topSite.clickUrl, TopSites.contileReportingUrl.testGetValue())

            topSiteImpressionPinged = true
        }

        controller.submitTopSitesImpressionPing(topSite, position)

        assertNotNull(TopSites.contileClick.testGetValue())

        val event = TopSites.contileClick.testGetValue()!!

        assertEquals(1, event.size)
        assertEquals("top_sites", event[0].category)
        assertEquals("contile_click", event[0].name)
        assertEquals("1", event[0].extra!!["position"])
        assertEquals("newtab", event[0].extra!!["source"])

        assertTrue(topSiteImpressionPinged)
    }

    @Test
    fun `WHEN the default Google top site is removed THEN the correct metric is recorded`() {
        val controller = spyk(createController())
        val topSite = TopSite.Default(
            id = 1L,
            title = "Google",
            url = SupportUtils.GOOGLE_URL,
            createdAt = 0,
        )
        assertNull(TopSites.remove.testGetValue())
        assertNull(TopSites.googleTopSiteRemoved.testGetValue())

        controller.handleRemoveTopSiteClicked(topSite)

        assertNotNull(TopSites.googleTopSiteRemoved.testGetValue())
        assertEquals(1, TopSites.googleTopSiteRemoved.testGetValue()!!.size)
        assertNull(TopSites.googleTopSiteRemoved.testGetValue()!!.single().extra)

        assertNotNull(TopSites.remove.testGetValue())
        assertEquals(1, TopSites.remove.testGetValue()!!.size)
        assertNull(TopSites.remove.testGetValue()!!.single().extra)
    }

    @Test
    fun `GIVEN exactly the required amount of downloaded thumbnails with no errors WHEN handling wallpaper dialog THEN dialog is shown`() {
        val wallpaperState = WallpaperState.default.copy(
            availableWallpapers = makeFakeRemoteWallpapers(
                THUMBNAILS_SELECTION_COUNT,
                false,
            ),
        )
        assertTrue(createController().handleShowWallpapersOnboardingDialog(wallpaperState))
    }

    @Test
    fun `GIVEN more than required amount of downloaded thumbnails with no errors WHEN handling wallpaper dialog THEN dialog is shown`() {
        val wallpaperState = WallpaperState.default.copy(
            availableWallpapers = makeFakeRemoteWallpapers(
                THUMBNAILS_SELECTION_COUNT,
                false,
            ),
        )
        assertTrue(createController().handleShowWallpapersOnboardingDialog(wallpaperState))
    }

    @Test
    fun `GIVEN more than required amount of downloaded thumbnails with some errors WHEN handling wallpaper dialog THEN dialog is shown`() {
        val wallpaperState = WallpaperState.default.copy(
            availableWallpapers = makeFakeRemoteWallpapers(
                THUMBNAILS_SELECTION_COUNT + 2,
                true,
            ),
        )
        assertTrue(createController().handleShowWallpapersOnboardingDialog(wallpaperState))
    }

    @Test
    fun `GIVEN fewer than the required amount of downloaded thumbnails WHEN handling wallpaper dialog THEN the dialog is not shown`() {
        val wallpaperState = WallpaperState.default.copy(
            availableWallpapers = makeFakeRemoteWallpapers(
                THUMBNAILS_SELECTION_COUNT - 1,
                false,
            ),
        )
        assertFalse(createController().handleShowWallpapersOnboardingDialog(wallpaperState))
    }

    @Test
    fun `GIVEN exactly the required amount of downloaded thumbnails with errors WHEN handling wallpaper dialog THEN the dialog is not shown`() {
        val wallpaperState = WallpaperState.default.copy(
            availableWallpapers = makeFakeRemoteWallpapers(
                THUMBNAILS_SELECTION_COUNT,
                true,
            ),
        )
        assertFalse(createController().handleShowWallpapersOnboardingDialog(wallpaperState))
    }

    @Test
    fun `GIVEN app is in private browsing mode WHEN handling wallpaper dialog THEN the dialog is not shown`() {
        every { activity.browsingModeManager } returns mockk {
            every { mode } returns mockk {
                every { isPrivate } returns true
            }
        }
        val wallpaperState = WallpaperState.default.copy(
            availableWallpapers = makeFakeRemoteWallpapers(
                THUMBNAILS_SELECTION_COUNT,
                true,
            ),
        )

        assertFalse(createController().handleShowWallpapersOnboardingDialog(wallpaperState))
    }

    @Test
    fun handleStartBrowsingClicked() {
        var hideOnboardingInvoked = false
        createController(hideOnboarding = { hideOnboardingInvoked = true }).handleStartBrowsingClicked()

        assertTrue(hideOnboardingInvoked)
    }

    @Test
    fun handleReadPrivacyNoticeClicked() {
        mockkObject(SupportUtils)
        val urlCaptor = slot<String>()
        every { SupportUtils.createCustomTabIntent(any(), capture(urlCaptor)) } returns mockk()

        createController().handleReadPrivacyNoticeClicked()

        verify {
            activity.startActivity(
                any(),
            )
        }
        assertEquals(
            SupportUtils.getMozillaPageUrl(SupportUtils.MozillaPage.PRIVATE_NOTICE),
            urlCaptor.captured,
        )
        unmockkObject(SupportUtils)
    }

    @Test
    fun handleToggleCollectionExpanded() {
        val collection = mockk<TabCollection>()
        createController().handleToggleCollectionExpanded(collection, true)
        verify { appStore.dispatch(AppAction.CollectionExpanded(collection, true)) }
    }

    @Test
    fun handleCreateCollection() {
        createController().handleCreateCollection()

        verify {
            navController.navigate(
                match<NavDirections> { it.actionId == R.id.action_global_tabsTrayFragment },
                null,
            )
        }
    }

    @Test
    fun handlePasteAndGo() {
        assertNull(Events.enteredUrl.testGetValue())
        assertNull(Events.performedSearch.testGetValue())

        createController().handlePasteAndGo("text")

        verify {
            activity.openToBrowserAndLoad(
                searchTermOrURL = "text",
                newTab = true,
                from = BrowserDirection.FromHome,
                engine = searchEngine,
            )
        }

        assertNotNull(Events.performedSearch.testGetValue())

        createController().handlePasteAndGo("https://mozilla.org")
        verify {
            activity.openToBrowserAndLoad(
                searchTermOrURL = "https://mozilla.org",
                newTab = true,
                from = BrowserDirection.FromHome,
                engine = searchEngine,
            )
        }
        assertNotNull(Events.enteredUrl.testGetValue())
    }

    @Test
    fun handlePaste() {
        createController().handlePaste("text")

        verify {
            navController.navigate(
                match<NavDirections> { it.actionId == R.id.action_global_search_dialog },
                null,
            )
        }
    }

    @Test
    fun handleRemoveCollectionsPlaceholder() {
        createController().handleRemoveCollectionsPlaceholder()

        verify {
            settings.showCollectionsPlaceholderOnHome = false
            appStore.dispatch(AppAction.RemoveCollectionsPlaceholder)
        }
    }

    @Test
    fun `WHEN private mode button is selected from home THEN handle mode change`() {
        every { navController.currentDestination } returns mockk {
            every { id } returns R.id.homeFragment
        }

        every { settings.incrementNumTimesPrivateModeOpened() } just Runs

        val newMode = BrowsingMode.Private
        val hasBeenOnboarded = true

        createController().handlePrivateModeButtonClicked(newMode, hasBeenOnboarded)

        verify {
            settings.incrementNumTimesPrivateModeOpened()
            AppAction.ModeChange(Mode.fromBrowsingMode(newMode))
        }
    }

    @Test
    fun `WHEN private mode is selected on home from behind search THEN handle mode change`() {
        every { navController.currentDestination } returns mockk {
            every { id } returns R.id.searchDialogFragment
        }

        every { settings.incrementNumTimesPrivateModeOpened() } just Runs

        val url = "https://mozilla.org"
        val tab = createTab(
            id = "otherTab",
            url = url,
            private = false,
            engineSession = mockk(relaxed = true),
        )
        store.dispatch(TabListAction.AddTabAction(tab, select = true)).joinBlocking()

        val newMode = BrowsingMode.Private
        val hasBeenOnboarded = true

        createController().handlePrivateModeButtonClicked(newMode, hasBeenOnboarded)

        verify {
            settings.incrementNumTimesPrivateModeOpened()
            AppAction.ModeChange(Mode.fromBrowsingMode(newMode))
        }
    }

    @Test
    fun `WHEN private mode is deselected on home from behind search THEN handle mode change`() {
        every { navController.currentDestination } returns mockk {
            every { id } returns R.id.searchDialogFragment
        }

        val url = "https://mozilla.org"
        val tab = createTab(
            id = "otherTab",
            url = url,
            private = true,
            engineSession = mockk(relaxed = true),
        )
        store.dispatch(TabListAction.AddTabAction(tab, select = true)).joinBlocking()

        val newMode = BrowsingMode.Normal
        val hasBeenOnboarded = true

        createController().handlePrivateModeButtonClicked(newMode, hasBeenOnboarded)

        verify(exactly = 0) {
            settings.incrementNumTimesPrivateModeOpened()
        }
        verify {
            appStore.dispatch(
                AppAction.ModeChange(Mode.fromBrowsingMode(newMode)),
            )
        }
    }

    @Test
    fun `WHEN handleReportSessionMetrics is called AND there are zero recent tabs THEN report Event#RecentTabsSectionIsNotVisible`() {
        assertNull(RecentTabs.sectionVisible.testGetValue())

        every { appState.recentTabs } returns emptyList()
        createController().handleReportSessionMetrics(appState)
        assertNotNull(RecentTabs.sectionVisible.testGetValue())
        assertFalse(RecentTabs.sectionVisible.testGetValue()!!)
    }

    @Test
    fun `WHEN handleReportSessionMetrics is called AND there is at least one recent tab THEN report Event#RecentTabsSectionIsVisible`() {
        assertNull(RecentTabs.sectionVisible.testGetValue())

        val recentTab: RecentTab = mockk(relaxed = true)
        every { appState.recentTabs } returns listOf(recentTab)
        createController().handleReportSessionMetrics(appState)

        assertNotNull(RecentTabs.sectionVisible.testGetValue())
        assertTrue(RecentTabs.sectionVisible.testGetValue()!!)
    }

    @Test
    fun `WHEN handleReportSessionMetrics is called AND there are zero recent bookmarks THEN report Event#RecentBookmarkCount(0)`() {
        every { appState.recentBookmarks } returns emptyList()
        every { appState.recentTabs } returns emptyList()
        assertNull(RecentBookmarks.recentBookmarksCount.testGetValue())

        createController().handleReportSessionMetrics(appState)

        assertNotNull(RecentBookmarks.recentBookmarksCount.testGetValue())
        assertEquals(0L, RecentBookmarks.recentBookmarksCount.testGetValue())
    }

    @Test
    fun `WHEN handleReportSessionMetrics is called AND there is at least one recent bookmark THEN report Event#RecentBookmarkCount(1)`() {
        val recentBookmark: RecentBookmark = mockk(relaxed = true)
        every { appState.recentBookmarks } returns listOf(recentBookmark)
        every { appState.recentTabs } returns emptyList()
        assertNull(RecentBookmarks.recentBookmarksCount.testGetValue())

        createController().handleReportSessionMetrics(appState)

        assertNotNull(RecentBookmarks.recentBookmarksCount.testGetValue())
        assertEquals(1L, RecentBookmarks.recentBookmarksCount.testGetValue())
    }

    @Test
    fun `WHEN handleTopSiteSettingsClicked is called THEN navigate to the HomeSettingsFragment AND report the interaction`() {
        createController().handleTopSiteSettingsClicked()

        assertNotNull(TopSites.contileSettings.testGetValue())
        assertEquals(1, TopSites.contileSettings.testGetValue()!!.size)
        assertNull(TopSites.contileSettings.testGetValue()!!.single().extra)
        verify {
            navController.navigate(
                match<NavDirections> {
                    it.actionId == R.id.action_global_homeSettingsFragment
                },
                null,
            )
        }
    }

    @Test
    fun `WHEN handleSponsorPrivacyClicked is called THEN navigate to the privacy webpage AND report the interaction`() {
        createController().handleSponsorPrivacyClicked()

        assertNotNull(TopSites.contileSponsorsAndPrivacy.testGetValue())
        assertEquals(1, TopSites.contileSponsorsAndPrivacy.testGetValue()!!.size)
        assertNull(TopSites.contileSponsorsAndPrivacy.testGetValue()!!.single().extra)
        verify {
            activity.openToBrowserAndLoad(
                searchTermOrURL = SupportUtils.getGenericSumoURLForTopic(SupportUtils.SumoTopic.SPONSOR_PRIVACY),
                newTab = true,
                from = BrowserDirection.FromHome,
            )
        }
    }

    @Test
    fun `WHEN handleOpenInPrivateTabClicked is called with a TopSite#Provided site THEN Event#TopSiteOpenContileInPrivateTab is reported`() {
        val topSite = TopSite.Provided(
            id = 1L,
            title = "Mozilla",
            url = "mozilla.org",
            clickUrl = "",
            imageUrl = "",
            impressionUrl = "",
            createdAt = 0,
        )
        createController().handleOpenInPrivateTabClicked(topSite)

        assertNotNull(TopSites.openContileInPrivateTab.testGetValue())
        assertEquals(1, TopSites.openContileInPrivateTab.testGetValue()!!.size)
        assertNull(TopSites.openContileInPrivateTab.testGetValue()!!.single().extra)
    }

    @Test
    fun `WHEN handleOpenInPrivateTabClicked is called with a Default, Pinned, or Frecent top site THEN openInPrivateTab event is recorded`() {
        val controller = createController()
        val topSite1 = TopSite.Default(
            id = 1L,
            title = "Mozilla",
            url = "mozilla.org",
            createdAt = 0,
        )
        val topSite2 = TopSite.Pinned(
            id = 1L,
            title = "Mozilla",
            url = "mozilla.org",
            createdAt = 0,
        )
        val topSite3 = TopSite.Frecent(
            id = 1L,
            title = "Mozilla",
            url = "mozilla.org",
            createdAt = 0,
        )
        assertNull(TopSites.openInPrivateTab.testGetValue())

        controller.handleOpenInPrivateTabClicked(topSite1)
        controller.handleOpenInPrivateTabClicked(topSite2)
        controller.handleOpenInPrivateTabClicked(topSite3)

        assertNotNull(TopSites.openInPrivateTab.testGetValue())
        assertEquals(3, TopSites.openInPrivateTab.testGetValue()!!.size)
        for (event in TopSites.openInPrivateTab.testGetValue()!!) {
            assertNull(event.extra)
        }
    }

    @Test
    fun `WHEN handleMessageClicked and handleMessageClosed are called THEN delegate to messageController`() {
        val controller = createController()
        val message = mockk<Message>()

        controller.handleMessageClicked(message)
        controller.handleMessageClosed(message)

        verify {
            messageController.onMessagePressed(message)
        }
        verify {
            messageController.onMessageDismissed(message)
        }
    }

    @Test
    fun `WHEN handleMenuItemTapped is called with SearchSettings item THEN navigate to SearchEngineFragment`() {
        createController().handleMenuItemTapped(SearchSelectorMenu.Item.SearchSettings)

        verify {
            navController.navigate(
                match<NavDirections> { it.actionId == R.id.action_global_searchEngineFragment },
                null,
            )
        }
    }

    @Test
    fun `WHEN handleMenuItemTapped is called with SearchEngine item THEN navigate to SearchDialogFragment`() {
        val item = mockk<SearchSelectorMenu.Item.SearchEngine>()
        every { item.searchEngine.id } returns "DuckDuckGo"

        createController().handleMenuItemTapped(item)

        val expectedDirections = HomeFragmentDirections.actionGlobalSearchDialog(
            sessionId = null,
            searchEngine = item.searchEngine.id,
        )
        verify {
            navController.navigate(expectedDirections, any<NavOptions>())
        }
    }

    private fun createController(
        hideOnboarding: () -> Unit = { },
        registerCollectionStorageObserver: () -> Unit = { },
        showTabTray: () -> Unit = { },
        removeCollectionWithUndo: (tabCollection: TabCollection) -> Unit = { },
    ): DefaultSessionControlController {
        return DefaultSessionControlController(
            activity = activity,
            settings = settings,
            engine = engine,
            store = store,
            messageController = messageController,
            tabCollectionStorage = tabCollectionStorage,
            addTabUseCase = tabsUseCases.addTab,
            restoreUseCase = mockk(relaxed = true),
            reloadUrlUseCase = reloadUrlUseCase.reload,
            selectTabUseCase = selectTabUseCase.selectTab,
            appStore = appStore,
            navController = navController,
            viewLifecycleScope = scope,
            hideOnboarding = hideOnboarding,
            registerCollectionStorageObserver = registerCollectionStorageObserver,
            removeCollectionWithUndo = removeCollectionWithUndo,
            showTabTray = showTabTray,
        )
    }

    private fun makeFakeRemoteWallpapers(size: Int, hasError: Boolean): List<Wallpaper> {
        val list = mutableListOf<Wallpaper>()
        for (i in 0 until size) {
            if (hasError && i == 0) {
                list.add(makeFakeRemoteWallpaper(Wallpaper.ImageFileState.Error))
            } else {
                list.add(makeFakeRemoteWallpaper(Wallpaper.ImageFileState.Downloaded))
            }
        }
        return list
    }

    private fun makeFakeRemoteWallpaper(
        thumbnailFileState: Wallpaper.ImageFileState = Wallpaper.ImageFileState.Unavailable,
    ) = Wallpaper(
        name = "name",
        collection = Wallpaper.Collection(
            name = Wallpaper.firefoxCollectionName,
            heading = null,
            description = null,
            availableLocales = null,
            startDate = null,
            endDate = null,
            learnMoreUrl = null,
        ),
        textColor = null,
        cardColorLight = null,
        cardColorDark = null,
        thumbnailFileState = thumbnailFileState,
        assetsFileState = Wallpaper.ImageFileState.Unavailable,
    )
}
