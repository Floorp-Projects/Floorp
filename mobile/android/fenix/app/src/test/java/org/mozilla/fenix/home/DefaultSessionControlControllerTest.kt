/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import androidx.navigation.NavController
import androidx.navigation.NavDirections
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.TestCoroutineScope
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.search.SearchEngineManager
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.Analytics
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.components.TopSiteStorage
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.components.tips.Tip
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.searchEngineManager
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.home.sessioncontrol.DefaultSessionControlController
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.utils.Settings
import mozilla.components.feature.tab.collections.Tab as ComponentTab

@OptIn(ExperimentalCoroutinesApi::class)
class DefaultSessionControlControllerTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(TestCoroutineDispatcher())

    private val activity: HomeActivity = mockk(relaxed = true)
    private val fragmentStore: HomeFragmentStore = mockk(relaxed = true)
    private val navController: NavController = mockk(relaxed = true)
    private val metrics: MetricController = mockk(relaxed = true)
    private val sessionManager: SessionManager = mockk(relaxed = true)
    private val engine: Engine = mockk(relaxed = true)
    private val tabCollectionStorage: TabCollectionStorage = mockk(relaxed = true)
    private val topSiteStorage: TopSiteStorage = mockk(relaxed = true)
    private val tabsUseCases: TabsUseCases = mockk(relaxed = true)
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
    private val searchEngine = mockk<SearchEngine>(relaxed = true)
    private val searchEngineManager = mockk<SearchEngineManager>(relaxed = true)
    private val settings: Settings = mockk(relaxed = true)
    private val analytics: Analytics = mockk(relaxed = true)

    private lateinit var controller: DefaultSessionControlController

    @Before
    fun setup() {
        every { fragmentStore.state } returns HomeFragmentState(
            collections = emptyList(),
            expandedCollections = emptySet(),
            mode = Mode.Normal,
            topSites = emptyList()
        )
        every { sessionManager.sessions } returns emptyList()
        every { navController.currentDestination } returns mockk {
            every { id } returns R.id.homeFragment
        }
        every { activity.components.settings } returns settings
        every { activity.components.search.provider.getDefaultEngine(activity) } returns searchEngine
        every { activity.settings() } returns settings
        every { activity.searchEngineManager } returns searchEngineManager
        every { searchEngineManager.defaultSearchEngine } returns searchEngine
        every { activity.components.analytics } returns analytics
        every { analytics.metrics } returns metrics

        controller = DefaultSessionControlController(
            activity = activity,
            engine = engine,
            metrics = metrics,
            sessionManager = sessionManager,
            tabCollectionStorage = tabCollectionStorage,
            topSiteStorage = topSiteStorage,
            addTabUseCase = tabsUseCases.addTab,
            fragmentStore = fragmentStore,
            navController = navController,
            viewLifecycleScope = TestCoroutineScope(),
            hideOnboarding = hideOnboarding,
            registerCollectionStorageObserver = registerCollectionStorageObserver,
            showDeleteCollectionPrompt = showDeleteCollectionPrompt,
            showTabTray = showTabTray,
            handleSwipedItemDeletionCancel = handleSwipedItemDeletionCancel
        )
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
                match<NavDirections> { it.actionId == R.id.action_global_collectionCreationFragment },
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
    fun `handleCollectionOpenTabClicked onTabRestored`() {
        val tab = mockk<ComponentTab> {
            every { restore(activity, engine, restoreSessionId = false) } returns mockk {
                every { session } returns mockk()
                every { engineSessionState } returns mockk()
            }
        }
        controller.handleCollectionOpenTabClicked(tab)

        verify { metrics.track(Event.CollectionTabRestored) }
        verify { activity.openToBrowser(BrowserDirection.FromHome) }
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

        controller.handleSelectTopSite(topSiteUrl, true)
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

        controller.handleSelectTopSite(topSiteUrl, false)
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
                match<NavDirections> { it.actionId == R.id.action_global_tabTrayDialogFragment },
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
            settings.incrementActiveSearchCount()
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
                match<NavDirections> { it.actionId == R.id.action_global_search },
                null
            )
        }
    }
}
