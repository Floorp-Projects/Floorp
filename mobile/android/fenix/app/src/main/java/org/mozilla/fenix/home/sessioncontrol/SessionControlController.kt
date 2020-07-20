/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol

import androidx.navigation.NavController
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.tab.collections.ext.restore
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.feature.top.sites.TopSite
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.collections.SaveCollectionStep
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.components.TopSiteStorage
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.components.tips.Tip
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.sessionsOfType
import org.mozilla.fenix.home.HomeFragment
import org.mozilla.fenix.home.HomeFragmentAction
import org.mozilla.fenix.home.HomeFragmentDirections
import org.mozilla.fenix.home.HomeFragmentStore
import org.mozilla.fenix.settings.SupportUtils
import mozilla.components.feature.tab.collections.Tab as ComponentTab

/**
 * [HomeFragment] controller. An interface that handles the view manipulation of the Tabs triggered
 * by the Interactor.
 */
@SuppressWarnings("TooManyFunctions")
interface SessionControlController {
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
     * @see [TopSiteInteractor.onSelectTopSite]
     */
    fun handleSelectTopSite(url: String, isDefault: Boolean)

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

    fun handleCloseTip(tip: Tip)

    /**
     * @see [CollectionInteractor.onAddTabsToCollectionTapped]
     */
    fun handleCreateCollection()
}

@SuppressWarnings("TooManyFunctions", "LargeClass", "LongParameterList")
class DefaultSessionControlController(
    private val activity: HomeActivity,
    private val engine: Engine,
    private val metrics: MetricController,
    private val sessionManager: SessionManager,
    private val tabCollectionStorage: TabCollectionStorage,
    private val topSiteStorage: TopSiteStorage,
    private val addTabUseCase: TabsUseCases.AddNewTabUseCase,
    private val fragmentStore: HomeFragmentStore,
    private val navController: NavController,
    private val viewLifecycleScope: CoroutineScope,
    private val hideOnboarding: () -> Unit,
    private val registerCollectionStorageObserver: () -> Unit,
    private val showDeleteCollectionPrompt: (tabCollection: TabCollection, title: String?, message: String) -> Unit,
    private val showTabTray: () -> Unit
) : SessionControlController {

    override fun handleCollectionAddTabTapped(collection: TabCollection) {
        metrics.track(Event.CollectionAddTabPressed)
        showCollectionCreationFragment(
            step = SaveCollectionStep.SelectTabs,
            selectedTabCollectionId = collection.id
        )
    }

    override fun handleCollectionOpenTabClicked(tab: ComponentTab) {
        sessionManager.restore(
            activity,
            engine,
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
        sessionManager.restore(
            activity,
            engine,
            collection,
            onFailure = { url ->
                addTabUseCase.invoke(url)
            }
        )

        showTabTray()
        metrics.track(Event.CollectionAllTabsRestored)
    }

    override fun handleCollectionRemoveTab(collection: TabCollection, tab: ComponentTab) {
        metrics.track(Event.CollectionTabRemoved)

        if (collection.tabs.size == 1) {
            val title = activity.resources.getString(R.string.delete_tab_and_collection_dialog_title, collection.title)
            val message = activity.resources.getString(R.string.delete_tab_and_collection_dialog_message)
            showDeleteCollectionPrompt(collection, title, message)
        } else {
            viewLifecycleScope.launch(Dispatchers.IO) {
                tabCollectionStorage.removeTabFromCollection(collection, tab)
            }
        }
    }

    override fun handleCollectionShareTabsClicked(collection: TabCollection) {
        showShareFragment(collection.tabs.map { ShareData(url = it.url, title = it.title) })
        metrics.track(Event.CollectionShared)
    }

    override fun handleDeleteCollectionTapped(collection: TabCollection) {
        val message = activity.resources.getString(R.string.tab_collection_dialog_message, collection.title)
        showDeleteCollectionPrompt(collection, null, message)
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

    override fun handleSelectTopSite(url: String, isDefault: Boolean) {
        metrics.track(Event.TopSiteOpenInNewTab)
        if (isDefault) { metrics.track(Event.TopSiteOpenDefault) }
        if (url == SupportUtils.POCKET_TRENDING_URL) { metrics.track(Event.PocketTopSiteClicked) }
        addTabUseCase.invoke(
            url = url,
            selectTab = true,
            startLoading = true
        )
        activity.openToBrowser(BrowserDirection.FromHome)
    }

    override fun handleStartBrowsingClicked() {
        hideOnboarding()
    }

    override fun handleOpenSettingsClicked() {
        val directions = HomeFragmentDirections.actionGlobalPrivateBrowsingFragment()
        navController.nav(R.id.homeFragment, directions)
    }

    override fun handleWhatsNewGetAnswersClicked() {
        activity.openToBrowserAndLoad(
            searchTermOrURL = SupportUtils.getWhatsNewUrl(activity),
            newTab = true,
            from = BrowserDirection.FromHome
        )
    }

    override fun handleReadPrivacyNoticeClicked() {
        activity.openToBrowserAndLoad(
            searchTermOrURL = SupportUtils.getMozillaPageUrl(SupportUtils.MozillaPage.PRIVATE_NOTICE),
            newTab = true,
            from = BrowserDirection.FromHome
        )
    }

    override fun handleToggleCollectionExpanded(collection: TabCollection, expand: Boolean) {
        fragmentStore.dispatch(HomeFragmentAction.CollectionExpanded(collection, expand))
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

        val tabIds = sessionManager
            .sessionsOfType(private = activity.browsingModeManager.mode.isPrivate)
            .map { session -> session.id }
            .toList()
            .toTypedArray()
        val directions = HomeFragmentDirections.actionGlobalCollectionCreationFragment(
            tabIds = tabIds,
            saveCollectionStep = step,
            selectedTabIds = selectedTabIds,
            selectedTabCollectionId = selectedTabCollectionId ?: -1
        )
        navController.nav(R.id.homeFragment, directions)
    }

    override fun handleCreateCollection() {
        showCollectionCreationFragment(step = SaveCollectionStep.SelectTabs)
    }

    private fun showShareFragment(data: List<ShareData>) {
        val directions = HomeFragmentDirections.actionGlobalShareFragment(
            data = data.toTypedArray()
        )
        navController.nav(R.id.homeFragment, directions)
    }
}
