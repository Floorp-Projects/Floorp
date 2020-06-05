/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import androidx.navigation.NavController
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.verify
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.newSingleThreadContext
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.setMain
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.tabs.TabsUseCases
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.home.sessioncontrol.DefaultSessionControlController
import org.mozilla.fenix.settings.SupportUtils
import mozilla.components.feature.tab.collections.Tab as ComponentTab

@ExperimentalCoroutinesApi
class DefaultSessionControlControllerTest {

    private val mainThreadSurrogate = newSingleThreadContext("UI thread")
    private val activity: HomeActivity = mockk(relaxed = true)
    private val fragmentStore: HomeFragmentStore = mockk(relaxed = true)
    private val navController: NavController = mockk(relaxed = true)
    private val getListOfTabs: () -> List<Tab> = { emptyList() }
    private val hideOnboarding: () -> Unit = mockk(relaxed = true)
    private val openSettingsScreen: () -> Unit = mockk(relaxed = true)
    private val openWhatsNewLink: () -> Unit = mockk(relaxed = true)
    private val openPrivacyNotice: () -> Unit = mockk(relaxed = true)
    private val registerCollectionStorageObserver: () -> Unit = mockk(relaxed = true)
    private val showTabTray: () -> Unit = mockk(relaxed = true)
    private val showDeleteCollectionPrompt: (tabCollection: TabCollection) -> Unit =
        mockk(relaxed = true)
    private val metrics: MetricController = mockk(relaxed = true)
    private val state: HomeFragmentState = mockk(relaxed = true)
    private val sessionManager: SessionManager = mockk(relaxed = true)
    private val engine: Engine = mockk(relaxed = true)
    private val tabCollectionStorage: TabCollectionStorage = mockk(relaxed = true)
    private val tabsUseCases: TabsUseCases = mockk(relaxed = true)

    private lateinit var controller: DefaultSessionControlController

    @Before
    fun setup() {
        Dispatchers.setMain(mainThreadSurrogate)
        mockkStatic("org.mozilla.fenix.ext.ContextKt")
        every { activity.components.core.engine } returns engine
        every { activity.components.core.sessionManager } returns sessionManager
        every { activity.components.core.tabCollectionStorage } returns tabCollectionStorage
        every { activity.components.useCases.tabsUseCases } returns tabsUseCases

        every { fragmentStore.state } returns state
        every { state.collections } returns emptyList()
        every { state.expandedCollections } returns emptySet()
        every { state.mode } returns Mode.Normal
        every { activity.components.analytics.metrics } returns metrics

        controller = DefaultSessionControlController(
            activity = activity,
            fragmentStore = fragmentStore,
            navController = navController,
            viewLifecycleScope = MainScope(),
            getListOfTabs = getListOfTabs,
            hideOnboarding = hideOnboarding,
            registerCollectionStorageObserver = registerCollectionStorageObserver,
            showDeleteCollectionPrompt = showDeleteCollectionPrompt,
            openSettingsScreen = openSettingsScreen,
            openWhatsNewLink = openWhatsNewLink,
            openPrivacyNotice = openPrivacyNotice,
            showTabTray = showTabTray
        )
    }

    @After
    fun tearDown() {
        Dispatchers.resetMain() // reset main dispatcher to the original Main dispatcher
        mainThreadSurrogate.close()
    }

    @Test
    fun handleCollectionAddTabTapped() {
        val collection: TabCollection = mockk(relaxed = true)
        controller.handleCollectionAddTabTapped(collection)
        verify { metrics.track(Event.CollectionAddTabPressed) }
    }

    @Test
    fun handleCollectionOpenTabClicked() {
        val tab: ComponentTab = mockk(relaxed = true)
        controller.handleCollectionOpenTabClicked(tab)
        verify { metrics.track(Event.CollectionTabRestored) }
    }

    @Test
    fun handleCollectionOpenTabsTapped() {
        val collection: TabCollection = mockk(relaxed = true)
        controller.handleCollectionOpenTabsTapped(collection)
        verify { metrics.track(Event.CollectionAllTabsRestored) }
    }

    @Test
    fun handleCollectionRemoveTab() {
        val collection: TabCollection = mockk(relaxed = true)
        val tab: ComponentTab = mockk(relaxed = true)
        controller.handleCollectionRemoveTab(collection, tab)
        verify { metrics.track(Event.CollectionTabRemoved) }
    }

    @Test
    fun handleCollectionShareTabsClicked() {
        val collection: TabCollection = mockk(relaxed = true)
        controller.handleCollectionShareTabsClicked(collection)
        verify { metrics.track(Event.CollectionShared) }
    }

    @Test
    fun handleDeleteCollectionTapped() {
        val collection: TabCollection = mockk(relaxed = true)
        controller.handleDeleteCollectionTapped(collection)
        verify { showDeleteCollectionPrompt(collection) }
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
        val collection: TabCollection = mockk(relaxed = true)
        controller.handleRenameCollectionTapped(collection)
        verify { metrics.track(Event.CollectionRenamePressed) }
    }

    @Test
    fun handleSelectDefaultTopSite() {
        val topSiteUrl = "mozilla.org"

        controller.handleSelectTopSite(topSiteUrl, true)
        verify { metrics.track(Event.TopSiteOpenInNewTab) }
        verify { metrics.track(Event.TopSiteOpenDefault) }
        verify { tabsUseCases.addTab.invoke(
            topSiteUrl,
            selectTab = true,
            startLoading = true
        ) }
        verify { activity.openToBrowser(BrowserDirection.FromHome) }
    }

    @Test
    fun handleSelectNonDefaultTopSite() {
        val topSiteUrl = "mozilla.org"

        controller.handleSelectTopSite(topSiteUrl, false)
        verify { metrics.track(Event.TopSiteOpenInNewTab) }
        verify { tabsUseCases.addTab.invoke(
            topSiteUrl,
            selectTab = true,
            startLoading = true
        ) }
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
        verify { openSettingsScreen() }
    }

    @Test
    fun handleToggleCollectionExpanded() {
        val collection: TabCollection = mockk(relaxed = true)
        controller.handleToggleCollectionExpanded(collection, true)
        verify { fragmentStore.dispatch(HomeFragmentAction.CollectionExpanded(collection, true)) }
    }
}
