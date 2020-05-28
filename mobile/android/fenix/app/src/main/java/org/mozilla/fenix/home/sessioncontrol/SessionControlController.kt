/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol

import android.view.View
import androidx.navigation.NavController
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.feature.media.ext.pauseIfPlaying
import mozilla.components.feature.media.ext.playIfPaused
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.tab.collections.ext.restore
import mozilla.components.feature.top.sites.TopSite
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.collections.SaveCollectionStep
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.components.TopSiteStorage
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.components.tips.Tip
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.sessionsOfType
import org.mozilla.fenix.home.HomeFragment
import org.mozilla.fenix.home.HomeFragmentAction
import org.mozilla.fenix.home.HomeFragmentDirections
import org.mozilla.fenix.home.HomeFragmentStore
import org.mozilla.fenix.home.Tab
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.settings.SupportUtils
import mozilla.components.feature.tab.collections.Tab as ComponentTab

/**
 * [HomeFragment] controller. An interface that handles the view manipulation of the Tabs triggered
 * by the Interactor.
 */
@SuppressWarnings("TooManyFunctions")
interface SessionControlController {
    /**
     * @see [TabSessionInteractor.onCloseTab]
     */
    fun handleCloseTab(sessionId: String)

    /**
     * @see [TabSessionInteractor.onCloseAllTabs]
     */
    fun handleCloseAllTabs(isPrivateMode: Boolean)

    /**
     * @see [CollectionInteractor.onCollectionAddTabTapped]
     */
    fun handleCollectionAddTabTapped(collection: TabCollection)

    /**
     * @see [CollectionInteractor.onCollectionOpenTabClicked]
     */
    fun handleCollectionOpenTabClicked(tab: ComponentTab)

    /**
     * @see [CollectionInteractor.onCollectionOpenTabsTapped]
     */
    fun handleCollectionOpenTabsTapped(collection: TabCollection)

    /**
     * @see [CollectionInteractor.onCollectionRemoveTab]
     */
    fun handleCollectionRemoveTab(collection: TabCollection, tab: ComponentTab)

    /**
     * @see [CollectionInteractor.onCollectionShareTabsClicked]
     */
    fun handleCollectionShareTabsClicked(collection: TabCollection)

    /**
     * @see [CollectionInteractor.onDeleteCollectionTapped]
     */
    fun handleDeleteCollectionTapped(collection: TabCollection)

    /**
     * @see [TopSiteInteractor.onOpenInPrivateTabClicked]
     */
    fun handleOpenInPrivateTabClicked(topSite: TopSite)

    /**
     * @see [TabSessionInteractor.onPauseMediaClicked]
     */
    fun handlePauseMediaClicked()

    /**
     * @see [TabSessionInteractor.onPlayMediaClicked]
     */
    fun handlePlayMediaClicked()

    /**
     * @see [TabSessionInteractor.onPrivateBrowsingLearnMoreClicked]
     */
    fun handlePrivateBrowsingLearnMoreClicked()

    /**
     * @see [TopSiteInteractor.onRemoveTopSiteClicked]
     */
    fun handleRemoveTopSiteClicked(topSite: TopSite)

    /**
     * @see [CollectionInteractor.onRenameCollectionTapped]
     */
    fun handleRenameCollectionTapped(collection: TabCollection)

    /**
     * @see [TabSessionInteractor.onSaveToCollection]
     */
    fun handleSaveTabToCollection(selectedTabId: String?)

    /**
     * @see [TabSessionInteractor.onSelectTab]
     */
    fun handleSelectTab(tabView: View, sessionId: String)

    /**
     * @see [TopSiteInteractor.onSelectTopSite]
     */
    fun handleSelectTopSite(url: String, isDefault: Boolean)

    /**
     * @see [TabSessionInteractor.onShareTabs]
     */
    fun handleShareTabs()

    /**
     * @see [OnboardingInteractor.onStartBrowsingClicked]
     */
    fun handleStartBrowsingClicked()

    /**
     * @see [OnboardingInteractor.onOpenSettingsClicked]
     */
    fun handleOpenSettingsClicked()

    /**
     * @see [OnboardingInteractor.onWhatsNewGetAnswersClicked]
     */
    fun handleWhatsNewGetAnswersClicked()

    /**
     * @see [OnboardingInteractor.onReadPrivacyNoticeClicked]
     */
    fun handleReadPrivacyNoticeClicked()

    /**
     * @see [CollectionInteractor.onToggleCollectionExpanded]
     */
    fun handleToggleCollectionExpanded(collection: TabCollection, expand: Boolean)

    /**
     * @see [TabSessionInteractor.onOpenNewTabClicked]
     */
    fun handleonOpenNewTabClicked()

    fun handleCloseTip(tip: Tip)
}

@SuppressWarnings("TooManyFunctions", "LargeClass")
class DefaultSessionControlController(
    private val store: BrowserStore,
    private val activity: HomeActivity,
    private val fragmentStore: HomeFragmentStore,
    private val navController: NavController,
    private val browsingModeManager: BrowsingModeManager,
    private val viewLifecycleScope: CoroutineScope,
    private val closeTab: (sessionId: String) -> Unit,
    private val closeAllTabs: (isPrivateMode: Boolean) -> Unit,
    private val getListOfTabs: () -> List<Tab>,
    private val hideOnboarding: () -> Unit,
    private val invokePendingDeleteJobs: () -> Unit,
    private val registerCollectionStorageObserver: () -> Unit,
    private val scrollToTheTop: () -> Unit,
    private val showDeleteCollectionPrompt: (tabCollection: TabCollection) -> Unit,
    private val openSettingsScreen: () -> Unit,
    private val openSearchScreen: () -> Unit,
    private val openWhatsNewLink: () -> Unit,
    private val openPrivacyNotice: () -> Unit,
    private val showTabTray: () -> Unit
) : SessionControlController {
    private val metrics: MetricController
        get() = activity.components.analytics.metrics
    private val sessionManager: SessionManager
        get() = activity.components.core.sessionManager
    private val tabCollectionStorage: TabCollectionStorage
        get() = activity.components.core.tabCollectionStorage
    private val topSiteStorage: TopSiteStorage
        get() = activity.components.core.topSiteStorage

    override fun handleCloseTab(sessionId: String) {
        closeTab.invoke(sessionId)
    }

    override fun handleCloseAllTabs(isPrivateMode: Boolean) {
        closeAllTabs.invoke(isPrivateMode)
    }

    override fun handleCollectionAddTabTapped(collection: TabCollection) {
        metrics.track(Event.CollectionAddTabPressed)
        showCollectionCreationFragment(
            step = SaveCollectionStep.SelectTabs,
            selectedTabCollectionId = collection.id
        )
    }

    override fun handleCollectionOpenTabClicked(tab: ComponentTab) {
        invokePendingDeleteJobs()

        sessionManager.restore(
            activity,
            activity.components.core.engine,
            tab,
            onTabRestored = {
                activity.openToBrowser(BrowserDirection.FromHome)
            },
            onFailure = {
                activity.openToBrowserAndLoad(
                    searchTermOrURL = tab.url,
                    newTab = true,
                    from = BrowserDirection.FromHome
                )
            }
        )

        metrics.track(Event.CollectionTabRestored)
    }

    override fun handleCollectionOpenTabsTapped(collection: TabCollection) {
        invokePendingDeleteJobs()

        sessionManager.restore(
            activity,
            activity.components.core.engine,
            collection,
            onFailure = { url ->
                activity.components.useCases.tabsUseCases.addTab.invoke(url)
            }
        )

        if (activity.settings().useNewTabTray) {
            showTabTray()
        } else {
            scrollToTheTop()
        }

        metrics.track(Event.CollectionAllTabsRestored)
    }

    override fun handleCollectionRemoveTab(collection: TabCollection, tab: ComponentTab) {
        metrics.track(Event.CollectionTabRemoved)

        viewLifecycleScope.launch(Dispatchers.IO) {
            tabCollectionStorage.removeTabFromCollection(collection, tab)
        }
    }

    override fun handleCollectionShareTabsClicked(collection: TabCollection) {
        showShareFragment(collection.tabs.map { ShareData(url = it.url, title = it.title) })
        metrics.track(Event.CollectionShared)
    }

    override fun handleDeleteCollectionTapped(collection: TabCollection) {
        showDeleteCollectionPrompt(collection)
    }

    override fun handleOpenInPrivateTabClicked(topSite: TopSite) {
        metrics.track(Event.TopSiteOpenInPrivateTab)
        with(activity) {
            browsingModeManager.mode = BrowsingMode.Private
            openToBrowserAndLoad(
                searchTermOrURL = topSite.url,
                newTab = true,
                from = BrowserDirection.FromHome
            )
        }
    }

    override fun handlePauseMediaClicked() {
        store.state.media.pauseIfPlaying()
    }

    override fun handlePlayMediaClicked() {
        store.state.media.playIfPaused()
    }

    override fun handlePrivateBrowsingLearnMoreClicked() {
        activity.openToBrowserAndLoad(
            searchTermOrURL = SupportUtils.getGenericSumoURLForTopic
                (SupportUtils.SumoTopic.PRIVATE_BROWSING_MYTHS),
            newTab = true,
            from = BrowserDirection.FromHome
        )
    }

    override fun handleRemoveTopSiteClicked(topSite: TopSite) {
        metrics.track(Event.TopSiteRemoved)
        if (topSite.url == SupportUtils.POCKET_TRENDING_URL) {
            metrics.track(Event.PocketTopSiteRemoved)
        }

        viewLifecycleScope.launch(Dispatchers.IO) {
            topSiteStorage.removeTopSite(topSite)
        }
    }

    override fun handleRenameCollectionTapped(collection: TabCollection) {
        showCollectionCreationFragment(
            step = SaveCollectionStep.RenameCollection,
            selectedTabCollectionId = collection.id
        )
        metrics.track(Event.CollectionRenamePressed)
    }

    override fun handleSaveTabToCollection(selectedTabId: String?) {
        if (browsingModeManager.mode.isPrivate) return

        invokePendingDeleteJobs()

        val tabs = getListOfTabs()
        val step = when {
            // Show the SelectTabs fragment if there are multiple opened tabs to select which tabs
            // you want to save to a collection.
            tabs.size > 1 -> SaveCollectionStep.SelectTabs
            // If there is an existing tab collection, show the SelectCollection fragment to save
            // the selected tab to a collection of your choice.
            tabCollectionStorage.cachedTabCollections.isNotEmpty() -> SaveCollectionStep.SelectCollection
            // Show the NameCollection fragment to create a new collection for the selected tab.
            else -> SaveCollectionStep.NameCollection
        }

        showCollectionCreationFragment(step, selectedTabId?.let { arrayOf(it) })
    }

    override fun handleSelectTab(tabView: View, sessionId: String) {
        invokePendingDeleteJobs()
        val session = sessionManager.findSessionById(sessionId)
        sessionManager.select(session!!)
        activity.openToBrowser(BrowserDirection.FromHome)
    }

    override fun handleSelectTopSite(url: String, isDefault: Boolean) {
        invokePendingDeleteJobs()
        metrics.track(Event.TopSiteOpenInNewTab)
        if (isDefault) { metrics.track(Event.TopSiteOpenDefault) }
        if (url == SupportUtils.POCKET_TRENDING_URL) { metrics.track(Event.PocketTopSiteClicked) }
        activity.components.useCases.tabsUseCases.addTab.invoke(
            url = url,
            selectTab = true,
            startLoading = true
        )
        activity.openToBrowser(BrowserDirection.FromHome)
    }

    override fun handleShareTabs() {
        invokePendingDeleteJobs()
        val shareData = sessionManager
            .sessionsOfType(private = browsingModeManager.mode.isPrivate)
            .map { ShareData(url = it.url, title = it.title) }
            .toList()
        showShareFragment(shareData)
    }

    override fun handleStartBrowsingClicked() {
        hideOnboarding()
    }

    override fun handleOpenSettingsClicked() {
        openSettingsScreen()
    }

    override fun handleWhatsNewGetAnswersClicked() {
        openWhatsNewLink()
    }

    override fun handleReadPrivacyNoticeClicked() {
        openPrivacyNotice()
    }

    override fun handleToggleCollectionExpanded(collection: TabCollection, expand: Boolean) {
        fragmentStore.dispatch(HomeFragmentAction.CollectionExpanded(collection, expand))
    }

    override fun handleonOpenNewTabClicked() {
        openSearchScreen()
    }

    override fun handleCloseTip(tip: Tip) {
        fragmentStore.dispatch(HomeFragmentAction.RemoveTip(tip))
    }

    private fun showCollectionCreationFragment(
        step: SaveCollectionStep,
        selectedTabIds: Array<String>? = null,
        selectedTabCollectionId: Long? = null
    ) {
        if (navController.currentDestination?.id == R.id.collectionCreationFragment) return

        // Only register the observer right before moving to collection creation
        registerCollectionStorageObserver()

        val tabIds = getListOfTabs().map { it.sessionId }.toTypedArray()
        val directions = HomeFragmentDirections.actionHomeFragmentToCreateCollectionFragment(
            tabIds = tabIds,
            previousFragmentId = R.id.homeFragment,
            saveCollectionStep = step,
            selectedTabIds = selectedTabIds,
            selectedTabCollectionId = selectedTabCollectionId ?: -1
        )
        navController.nav(R.id.homeFragment, directions)
    }

    private fun showShareFragment(data: List<ShareData>) {
        val directions = HomeFragmentDirections.actionGlobalShareFragment(
            data = data.toTypedArray()
        )
        navController.nav(R.id.homeFragment, directions)
    }

    companion object {
        private const val TAB_ITEM_TRANSITION_NAME = "tab_item"
    }
}
