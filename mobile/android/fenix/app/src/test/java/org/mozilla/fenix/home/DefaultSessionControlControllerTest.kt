/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import androidx.navigation.NavController
import androidx.navigation.NavDirections
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.spyk
import io.mockk.unmockkStatic
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.TestCoroutineScope
import mozilla.components.browser.state.action.SearchAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.search.RegionState
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ReaderState
import mozilla.components.browser.state.state.SearchState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.state.recover.RecoverableTab
import mozilla.components.browser.state.state.selectedOrDefaultSearchEngine
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.Analytics
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.Event.PerformedSearch.EngineSource
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.components.tips.Tip
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.home.sessioncontrol.DefaultSessionControlController
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.utils.Settings
import mozilla.components.feature.tab.collections.Tab as ComponentTab

@OptIn(ExperimentalCoroutinesApi::class)
class DefaultSessionControlControllerTest {

    private val testDispatcher = TestCoroutineDispatcher()

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(testDispatcher)

    private val activity: HomeActivity = mockk(relaxed = true)
    private val fragmentStore: HomeFragmentStore = mockk(relaxed = true)
    private val navController: NavController = mockk(relaxed = true)
    private val metrics: MetricController = mockk(relaxed = true)
    private val engine: Engine = mockk(relaxed = true)
    private val tabCollectionStorage: TabCollectionStorage = mockk(relaxed = true)
    private val tabsUseCases: TabsUseCases = mockk(relaxed = true)
    private val reloadUrlUseCase: SessionUseCases = mockk(relaxed = true)
    private val selectTabUseCase: TabsUseCases = mockk(relaxed = true)
    private val hideOnboarding: () -> Unit = mockk(relaxed = true)
    private val registerCollectionStorageObserver: () -> Unit = mockk(relaxed = true)
    private val showTabTray: () -> Unit = mockk(relaxed = true)
    private val handleSwipedItemDeletionCancel: () -> Unit = mockk(relaxed = true)
    private val showDeleteCollectionPrompt: (
        tabCollection: TabCollection,
        title: String?,
        message: String,
        wasSwiped: Boolean,
        handleSwipedItemDeletionCancel: () -> Unit
    ) -> Unit = mockk(relaxed = true)
    private val settings: Settings = mockk(relaxed = true)
    private val analytics: Analytics = mockk(relaxed = true)
    private val scope = TestCoroutineScope()
    private val searchEngine = SearchEngine(
        id = "test",
        name = "Test Engine",
        icon = mockk(relaxed = true),
        type = SearchEngine.Type.BUNDLED,
        resultUrls = listOf("https://example.org/?q={searchTerms}")
    )

    private val googleSearchEngine = SearchEngine(
        id = "googleTest",
        name = "Google Test Engine",
        icon = mockk(relaxed = true),
        type = SearchEngine.Type.BUNDLED,
        resultUrls = listOf("https://www.google.com/?q={searchTerms}"),
        suggestUrl = "https://www.google.com/"
    )

    private val duckDuckGoSearchEngine = SearchEngine(
        id = "ddgTest",
        name = "DuckDuckGo Test Engine",
        icon = mockk(relaxed = true),
        type = SearchEngine.Type.BUNDLED,
        resultUrls = listOf("https://duckduckgo.com/?q=%7BsearchTerms%7D&t=fpas"),
        suggestUrl = "https://ac.duckduckgo.com/ac/?q=%7BsearchTerms%7D&type=list"
    )

    private lateinit var store: BrowserStore
    private lateinit var controller: DefaultSessionControlController

    @Before
    fun setup() {
        store = BrowserStore(
            BrowserState(
                search = SearchState(
                    regionSearchEngines = listOf(searchEngine)
                )
            )
        )

        every { fragmentStore.state } returns HomeFragmentState(
            collections = emptyList(),
            expandedCollections = emptySet(),
            mode = Mode.Normal,
            topSites = emptyList(),
            showCollectionPlaceholder = true,
            showSetAsDefaultBrowserCard = true,
            recentTabs = emptyList(),
            recentBookmarks = emptyList()
        )

        every { navController.currentDestination } returns mockk {
            every { id } returns R.id.homeFragment
        }
        every { activity.components.settings } returns settings
        every { activity.settings() } returns settings
        every { activity.components.analytics } returns analytics
        every { analytics.metrics } returns metrics

        val restoreUseCase: TabsUseCases.RestoreUseCase = mockk(relaxed = true)

        controller = spyk(DefaultSessionControlController(
            activity = activity,
            store = store,
            settings = settings,
            engine = engine,
            metrics = metrics,
            tabCollectionStorage = tabCollectionStorage,
            addTabUseCase = tabsUseCases.addTab,
            reloadUrlUseCase = reloadUrlUseCase.reload,
            selectTabUseCase = selectTabUseCase.selectTab,
            restoreUseCase = restoreUseCase,
            fragmentStore = fragmentStore,
            navController = navController,
            viewLifecycleScope = scope,
            hideOnboarding = hideOnboarding,
            registerCollectionStorageObserver = registerCollectionStorageObserver,
            showDeleteCollectionPrompt = showDeleteCollectionPrompt,
            showTabTray = showTabTray,
            handleSwipedItemDeletionCancel = handleSwipedItemDeletionCancel
        ))
    }

    @After
    fun cleanUp() {
        scope.cleanupTestCoroutines()
        testDispatcher.cleanupTestCoroutines()
    }

    @Test
    fun handleCollectionAddTabTapped() {
        val collection = mockk<TabCollection> {
            every { id } returns 12L
        }
        controller.handleCollectionAddTabTapped(collection)

        verify { metrics.track(Event.CollectionAddTabPressed) }
        verify {
            navController.navigate(
                match<NavDirections> {
                    it.actionId == R.id.action_global_collectionCreationFragment
                },
                null
            )
        }
    }

    @Test
    fun `handleCollectionOpenTabClicked onFailure`() {
        val tab = mockk<ComponentTab> {
            every { url } returns "https://mozilla.org"
            every { restore(activity, engine, restoreSessionId = false) } returns null
        }
        controller.handleCollectionOpenTabClicked(tab)

        verify { metrics.track(Event.CollectionTabRestored) }
        verify {
            activity.openToBrowserAndLoad(
                searchTermOrURL = "https://mozilla.org",
                newTab = true,
                from = BrowserDirection.FromHome
            )
        }
    }

    @Test
    fun `handleCollectionOpenTabClicked with existing selected tab`() {
        val recoverableTab = RecoverableTab(
            id = "test",
            parentId = null,
            url = "https://www.mozilla.org",
            title = "Mozilla",
            state = null,
            contextId = null,
            readerState = ReaderState(),
            lastAccess = 0,
            private = false
        )

        val tab = mockk<ComponentTab> {
            every { restore(activity, engine, restoreSessionId = false) } returns recoverableTab
        }

        val restoredTab = createTab(id = recoverableTab.id, url = recoverableTab.url)
        val otherTab = createTab(id = "otherTab", url = "https://mozilla.org")
        store.dispatch(TabListAction.AddTabAction(otherTab)).joinBlocking()
        store.dispatch(TabListAction.SelectTabAction(otherTab.id)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(restoredTab)).joinBlocking()

        controller.handleCollectionOpenTabClicked(tab)
        verify { metrics.track(Event.CollectionTabRestored) }
        verify { activity.openToBrowser(BrowserDirection.FromHome) }
        verify { selectTabUseCase.selectTab.invoke(restoredTab.id) }
        verify { reloadUrlUseCase.reload.invoke(restoredTab.id) }
    }

    @Test
    fun `handleCollectionOpenTabClicked without existing selected tab`() {
        val recoverableTab = RecoverableTab(
            id = "test",
            parentId = null,
            url = "https://www.mozilla.org",
            title = "Mozilla",
            state = null,
            contextId = null,
            readerState = ReaderState(),
            lastAccess = 0,
            private = false
        )

        val tab = mockk<ComponentTab> {
            every { restore(activity, engine, restoreSessionId = false) } returns recoverableTab
        }

        val restoredTab = createTab(id = recoverableTab.id, url = recoverableTab.url)
        store.dispatch(TabListAction.AddTabAction(restoredTab)).joinBlocking()

        controller.handleCollectionOpenTabClicked(tab)
        verify { metrics.track(Event.CollectionTabRestored) }
        verify { activity.openToBrowser(BrowserDirection.FromHome) }
        verify { selectTabUseCase.selectTab.invoke(restoredTab.id) }
        verify { reloadUrlUseCase.reload.invoke(restoredTab.id) }
    }

    @Test
    fun handleCollectionOpenTabsTapped() {
        val collection = mockk<TabCollection> {
            every { tabs } returns emptyList()
        }
        controller.handleCollectionOpenTabsTapped(collection)

        verify { metrics.track(Event.CollectionAllTabsRestored) }
    }

    @Test
    fun `handleCollectionRemoveTab one tab`() {
        val collection = mockk<TabCollection> {
            every { tabs } returns listOf(mockk())
            every { title } returns "Collection"
        }
        val tab = mockk<ComponentTab>()
        every {
            activity.resources.getString(
                R.string.delete_tab_and_collection_dialog_title,
                "Collection"
            )
        } returns "Delete Collection?"
        every {
            activity.resources.getString(R.string.delete_tab_and_collection_dialog_message)
        } returns "Deleting this tab will delete everything."

        controller.handleCollectionRemoveTab(collection, tab, false)

        verify { metrics.track(Event.CollectionTabRemoved) }
        verify {
            showDeleteCollectionPrompt(
                collection,
                "Delete Collection?",
                "Deleting this tab will delete everything.",
                false,
                handleSwipedItemDeletionCancel
            )
        }
    }

    @Test
    fun `handleCollectionRemoveTab multiple tabs`() {
        val collection: TabCollection = mockk(relaxed = true)
        val tab: ComponentTab = mockk(relaxed = true)
        controller.handleCollectionRemoveTab(collection, tab, false)
        verify { metrics.track(Event.CollectionTabRemoved) }
    }

    @Test
    fun handleCollectionShareTabsClicked() {
        val collection = mockk<TabCollection> {
            every { tabs } returns emptyList()
            every { title } returns ""
        }
        controller.handleCollectionShareTabsClicked(collection)

        verify { metrics.track(Event.CollectionShared) }
        verify {
            navController.navigate(
                match<NavDirections> { it.actionId == R.id.action_global_shareFragment },
                null
            )
        }
    }

    @Test
    fun handleDeleteCollectionTapped() {
        val collection = mockk<TabCollection> {
            every { title } returns "Collection"
        }
        every {
            activity.resources.getString(R.string.tab_collection_dialog_message, "Collection")
        } returns "Are you sure you want to delete Collection?"

        controller.handleDeleteCollectionTapped(collection)
        verify {
            showDeleteCollectionPrompt(
                collection,
                null,
                "Are you sure you want to delete Collection?",
                false,
                handleSwipedItemDeletionCancel
            )
        }
    }

    @Test
    fun handlePrivateBrowsingLearnMoreClicked() {
        controller.handlePrivateBrowsingLearnMoreClicked()
        verify {
            activity.openToBrowserAndLoad(
                searchTermOrURL = SupportUtils.getGenericSumoURLForTopic
                    (SupportUtils.SumoTopic.PRIVATE_BROWSING_MYTHS),
                newTab = true,
                from = BrowserDirection.FromHome
            )
        }
    }

    @Test
    fun handleRenameCollectionTapped() {
        val collection = mockk<TabCollection> {
            every { id } returns 3L
        }
        controller.handleRenameCollectionTapped(collection)

        verify { metrics.track(Event.CollectionRenamePressed) }
        verify {
            navController.navigate(
                match<NavDirections> { it.actionId == R.id.action_global_collectionCreationFragment },
                null
            )
        }
    }

    @Test
    fun handleSelectDefaultTopSite() {
        val topSiteUrl = "mozilla.org"
        every { controller.getAvailableSearchEngines() } returns listOf(searchEngine)

        controller.handleSelectTopSite(topSiteUrl, TopSite.Type.DEFAULT)
        verify { metrics.track(Event.TopSiteOpenInNewTab) }
        verify { metrics.track(Event.TopSiteOpenDefault) }
        verify {
            tabsUseCases.addTab.invoke(
                topSiteUrl,
                selectTab = true,
                startLoading = true
            )
        }
        verify { activity.openToBrowser(BrowserDirection.FromHome) }
    }

    @Test
    fun handleSelectNonDefaultTopSite() {
        val topSiteUrl = "mozilla.org"
        every { controller.getAvailableSearchEngines() } returns listOf(searchEngine)

        controller.handleSelectTopSite(topSiteUrl, TopSite.Type.FRECENT)
        verify { metrics.track(Event.TopSiteOpenInNewTab) }
        verify {
            tabsUseCases.addTab.invoke(
                topSiteUrl,
                selectTab = true,
                startLoading = true
            )
        }
        verify { activity.openToBrowser(BrowserDirection.FromHome) }
    }

    @Test
    fun handleSelectGoogleDefaultTopSiteUS() {
        val topSiteUrl = SupportUtils.GOOGLE_URL
        every { controller.getAvailableSearchEngines() } returns listOf(searchEngine)

        store.dispatch(SearchAction.SetRegionAction(RegionState("US", "US"))).joinBlocking()

        controller.handleSelectTopSite(topSiteUrl, TopSite.Type.DEFAULT)
        verify { metrics.track(Event.TopSiteOpenInNewTab) }
        verify { metrics.track(Event.TopSiteOpenDefault) }
        verify { metrics.track(Event.TopSiteOpenGoogle) }
        verify {
            tabsUseCases.addTab.invoke(
                url = SupportUtils.GOOGLE_US_URL,
                selectTab = true,
                startLoading = true
            )
        }
        verify { activity.openToBrowser(BrowserDirection.FromHome) }
    }

    @Test
    fun handleSelectGoogleDefaultTopSiteXX() {
        val topSiteUrl = SupportUtils.GOOGLE_URL
        every { controller.getAvailableSearchEngines() } returns listOf(searchEngine)

        store.dispatch(SearchAction.SetRegionAction(RegionState("DE", "FR"))).joinBlocking()

        controller.handleSelectTopSite(topSiteUrl, TopSite.Type.DEFAULT)
        verify { metrics.track(Event.TopSiteOpenInNewTab) }
        verify { metrics.track(Event.TopSiteOpenDefault) }
        verify { metrics.track(Event.TopSiteOpenGoogle) }
        verify {
            tabsUseCases.addTab.invoke(
                SupportUtils.GOOGLE_XX_URL,
                selectTab = true,
                startLoading = true
            )
        }
        verify { activity.openToBrowser(BrowserDirection.FromHome) }
    }

    @Test
    fun handleSelectGoogleDefaultTopSite_EventPerformedSearchTopSite() {
        val topSiteUrl = SupportUtils.GOOGLE_URL
        val engineSource = EngineSource.Default(googleSearchEngine, false)
        every { controller.getAvailableSearchEngines() } returns listOf(googleSearchEngine)
        try {
            mockkStatic("mozilla.components.browser.state.state.SearchStateKt")

            every { any<SearchState>().selectedOrDefaultSearchEngine } returns googleSearchEngine

            controller.handleSelectTopSite(topSiteUrl, TopSite.Type.DEFAULT)

            verify {
                metrics.track(
                    Event.PerformedSearch(
                        Event.PerformedSearch.EventSource.TopSite(
                            engineSource
                        )
                    )
                )
                metrics.track(Event.TopSiteOpenGoogle)
                metrics.track(Event.TopSiteOpenDefault)
            }
        } finally {
            unmockkStatic("mozilla.components.browser.state.state.SearchStateKt")
        }
    }

    @Test
    fun handleSelectDuckDuckGoTopSite_EventPerformedSearchTopSite() {
        val topSiteUrl = "https://duckduckgo.com"
        val engineSource = EngineSource.Shortcut(duckDuckGoSearchEngine, false)
        every { controller.getAvailableSearchEngines() } returns listOf(googleSearchEngine, duckDuckGoSearchEngine)
        try {
            mockkStatic("mozilla.components.browser.state.state.SearchStateKt")

            every { any<SearchState>().selectedOrDefaultSearchEngine } returns googleSearchEngine

            controller.handleSelectTopSite(topSiteUrl, TopSite.Type.PINNED)

            verify {
                metrics.track(
                    Event.PerformedSearch(
                        Event.PerformedSearch.EventSource.TopSite(
                            engineSource
                        )
                    )
                )

                metrics.track(Event.TopSiteOpenPinned)
            }
        } finally {
            unmockkStatic("mozilla.components.browser.state.state.SearchStateKt")
        }
    }

    @Test
    fun handleSelectGooglePinnedTopSiteUS() {
        val topSiteUrl = SupportUtils.GOOGLE_URL
        every { controller.getAvailableSearchEngines() } returns listOf(searchEngine)

        store.dispatch(SearchAction.SetRegionAction(RegionState("US", "US"))).joinBlocking()

        controller.handleSelectTopSite(topSiteUrl, TopSite.Type.PINNED)
        verify { metrics.track(Event.TopSiteOpenInNewTab) }
        verify { metrics.track(Event.TopSiteOpenPinned) }
        verify { metrics.track(Event.TopSiteOpenGoogle) }
        verify {
            tabsUseCases.addTab.invoke(
                SupportUtils.GOOGLE_US_URL,
                selectTab = true,
                startLoading = true
            )
        }
        verify { activity.openToBrowser(BrowserDirection.FromHome) }
    }

    @Test
    fun handleSelectGooglePinnedTopSiteXX() {
        val topSiteUrl = SupportUtils.GOOGLE_URL
        every { controller.getAvailableSearchEngines() } returns listOf(searchEngine)

        store.dispatch(SearchAction.SetRegionAction(RegionState("DE", "FR"))).joinBlocking()

        controller.handleSelectTopSite(topSiteUrl, TopSite.Type.PINNED)
        verify { metrics.track(Event.TopSiteOpenInNewTab) }
        verify { metrics.track(Event.TopSiteOpenPinned) }
        verify { metrics.track(Event.TopSiteOpenGoogle) }
        verify {
            tabsUseCases.addTab.invoke(
                SupportUtils.GOOGLE_XX_URL,
                selectTab = true,
                startLoading = true
            )
        }
        verify { activity.openToBrowser(BrowserDirection.FromHome) }
    }

    @Test
    fun handleSelectGoogleFrecentTopSiteUS() {
        val topSiteUrl = SupportUtils.GOOGLE_URL
        every { controller.getAvailableSearchEngines() } returns listOf(searchEngine)

        store.dispatch(SearchAction.SetRegionAction(RegionState("US", "US"))).joinBlocking()

        controller.handleSelectTopSite(topSiteUrl, TopSite.Type.FRECENT)
        verify { metrics.track(Event.TopSiteOpenInNewTab) }
        verify { metrics.track(Event.TopSiteOpenFrecent) }
        verify { metrics.track(Event.TopSiteOpenGoogle) }
        verify {
            tabsUseCases.addTab.invoke(
                SupportUtils.GOOGLE_US_URL,
                selectTab = true,
                startLoading = true
            )
        }
        verify { activity.openToBrowser(BrowserDirection.FromHome) }
    }

    @Test
    fun handleSelectGoogleFrecentTopSiteXX() {
        val topSiteUrl = SupportUtils.GOOGLE_URL
        every { controller.getAvailableSearchEngines() } returns listOf(searchEngine)

        store.dispatch(SearchAction.SetRegionAction(RegionState("DE", "FR"))).joinBlocking()

        controller.handleSelectTopSite(topSiteUrl, TopSite.Type.FRECENT)
        verify { metrics.track(Event.TopSiteOpenInNewTab) }
        verify { metrics.track(Event.TopSiteOpenFrecent) }
        verify { metrics.track(Event.TopSiteOpenGoogle) }
        verify {
            tabsUseCases.addTab.invoke(
                SupportUtils.GOOGLE_XX_URL,
                selectTab = true,
                startLoading = true
            )
        }
        verify { activity.openToBrowser(BrowserDirection.FromHome) }
    }

    @Test
    fun handleStartBrowsingClicked() {
        controller.handleStartBrowsingClicked()
        verify { hideOnboarding() }
    }

    @Test
    fun handleOpenSettingsClicked() {
        controller.handleOpenSettingsClicked()
        verify {
            navController.navigate(
                match<NavDirections> { it.actionId == R.id.action_global_privateBrowsingFragment },
                null
            )
        }
    }

    @Test
    fun handleWhatsNewGetAnswersClicked() {
        controller.handleWhatsNewGetAnswersClicked()
        val whatsNewUrl = SupportUtils.getWhatsNewUrl(activity)
        verify {
            activity.openToBrowserAndLoad(
                searchTermOrURL = whatsNewUrl,
                newTab = true,
                from = BrowserDirection.FromHome
            )
        }
    }

    @Test
    fun handleReadPrivacyNoticeClicked() {
        controller.handleReadPrivacyNoticeClicked()
        verify {
            activity.openToBrowserAndLoad(
                searchTermOrURL = SupportUtils.getMozillaPageUrl(SupportUtils.MozillaPage.PRIVATE_NOTICE),
                newTab = true,
                from = BrowserDirection.FromHome
            )
        }
    }

    @Test
    fun handleToggleCollectionExpanded() {
        val collection = mockk<TabCollection>()
        controller.handleToggleCollectionExpanded(collection, true)
        verify { fragmentStore.dispatch(HomeFragmentAction.CollectionExpanded(collection, true)) }
    }

    @Test
    fun handleCloseTip() {
        val tip = mockk<Tip>()
        controller.handleCloseTip(tip)
        verify { fragmentStore.dispatch(HomeFragmentAction.RemoveTip(tip)) }
    }

    @Test
    fun handleCreateCollection() {
        controller.handleCreateCollection()

        verify {
            navController.navigate(
                match<NavDirections> { it.actionId == R.id.action_global_tabsTrayFragment },
                null
            )
        }
    }

    @Test
    fun handlePasteAndGo() {
        controller.handlePasteAndGo("text")

        verify {
            activity.openToBrowserAndLoad(
                searchTermOrURL = "text",
                newTab = true,
                from = BrowserDirection.FromHome,
                engine = searchEngine
            )
            metrics.track(any<Event.PerformedSearch>())
        }

        controller.handlePasteAndGo("https://mozilla.org")
        verify {
            activity.openToBrowserAndLoad(
                searchTermOrURL = "https://mozilla.org",
                newTab = true,
                from = BrowserDirection.FromHome,
                engine = searchEngine
            )
            metrics.track(any<Event.EnteredUrl>())
        }
    }

    @Test
    fun handlePaste() {
        controller.handlePaste("text")

        verify {
            navController.navigate(
                match<NavDirections> { it.actionId == R.id.action_global_search_dialog },
                null
            )
        }
    }

    @Test
    fun handleRemoveCollectionsPlaceholder() {
        controller.handleRemoveCollectionsPlaceholder()

        verify {
            settings.showCollectionsPlaceholderOnHome = false
            fragmentStore.dispatch(HomeFragmentAction.RemoveCollectionsPlaceholder)
        }
    }

    @Test
    fun handleMenuOpenedWhileSearchShowing() {
        every { navController.currentDestination } returns mockk {
            every { id } returns R.id.searchDialogFragment
        }

        controller.handleMenuOpened()

        verify {
            navController.navigateUp()
        }
    }

    @Test
    fun handleMenuOpenedWhileSearchNotShowing() {
        every { navController.currentDestination } returns mockk {
            every { id } returns R.id.homeFragment
        }

        controller.handleMenuOpened()

        verify(exactly = 0) {
            navController.navigateUp()
        }
    }
}
