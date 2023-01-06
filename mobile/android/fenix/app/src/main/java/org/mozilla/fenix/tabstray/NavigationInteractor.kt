/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import android.content.Context
import androidx.navigation.NavController
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import mozilla.components.browser.state.selector.getNormalOrPrivateTabs
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.glean.private.NoExtras
import org.mozilla.fenix.GleanMetrics.Collections
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.GleanMetrics.TabsTray
import org.mozilla.fenix.collections.CollectionsDialog
import org.mozilla.fenix.collections.show
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.components.bookmarks.BookmarksUseCase
import org.mozilla.fenix.home.HomeFragment
import org.mozilla.fenix.tabstray.ext.getTabSessionState
import org.mozilla.fenix.tabstray.ext.isActiveDownload
import kotlin.coroutines.CoroutineContext

/**
 * An interactor that helps with navigating to different parts of the app from the tabs tray.
 */
@Suppress("TooManyFunctions")
interface NavigationInteractor {

    /**
     * Called when tab tray should be dismissed.
     */
    fun onTabTrayDismissed()

    /**
     * Called when clicking the account settings button.
     */
    fun onAccountSettingsClicked()

    /**
     * Called when sharing a list of [TabSessionState]s.
     */
    fun onShareTabs(tabs: Collection<TabSessionState>)

    /**
     * Called when clicking the share tabs button.
     */
    fun onShareTabsOfTypeClicked(private: Boolean)

    /**
     * Called when clicking the tab settings button.
     */
    fun onTabSettingsClicked()

    /**
     * Called when clicking the close all tabs button.
     */
    fun onCloseAllTabsClicked(private: Boolean)

    /**
     * Called when cancelling private downloads confirmed.
     */
    fun onCloseAllPrivateTabsWarningConfirmed(private: Boolean)

    /**
     * Called when opening the recently closed tabs menu button.
     */
    fun onOpenRecentlyClosedClicked()

    /**
     * Used when opening the add-to-collections user flow.
     */
    fun onSaveToCollections(tabs: Collection<TabSessionState>)

    /**
     * Used when adding [TabSessionState]s as bookmarks.
     */
    fun onSaveToBookmarks(tabs: Collection<TabSessionState>)
}

/**
 * A default implementation of [NavigationInteractor].
 */
@Suppress("LongParameterList", "TooManyFunctions")
class DefaultNavigationInteractor(
    private val context: Context,
    private val browserStore: BrowserStore,
    private val navController: NavController,
    private val dismissTabTray: () -> Unit,
    private val dismissTabTrayAndNavigateHome: (sessionId: String) -> Unit,
    private val bookmarksUseCase: BookmarksUseCase,
    private val tabsTrayStore: TabsTrayStore,
    private val collectionStorage: TabCollectionStorage,
    private val showCollectionSnackbar: (
        tabSize: Int,
        isNewCollection: Boolean,
    ) -> Unit,
    private val showBookmarkSnackbar: (tabSize: Int) -> Unit,
    private val showCancelledDownloadWarning: (downloadCount: Int, tabId: String?, source: String?) -> Unit,
    private val accountManager: FxaAccountManager,
    private val ioDispatcher: CoroutineContext,
) : NavigationInteractor {

    override fun onTabTrayDismissed() {
        TabsTray.closed.record(NoExtras())
        dismissTabTray()
    }

    override fun onAccountSettingsClicked() {
        val isSignedIn = accountManager.authenticatedAccount() != null

        val direction = if (isSignedIn) {
            TabsTrayFragmentDirections.actionGlobalAccountSettingsFragment()
        } else {
            TabsTrayFragmentDirections.actionGlobalTurnOnSync()
        }
        navController.navigate(direction)
    }

    override fun onTabSettingsClicked() {
        navController.navigate(
            TabsTrayFragmentDirections.actionGlobalTabSettingsFragment(),
        )
    }

    override fun onOpenRecentlyClosedClicked() {
        navController.navigate(
            TabsTrayFragmentDirections.actionGlobalRecentlyClosed(),
        )
        Events.recentlyClosedTabsOpened.record(NoExtras())
    }

    override fun onShareTabs(tabs: Collection<TabSessionState>) {
        TabsTray.shareSelectedTabs.record(TabsTray.ShareSelectedTabsExtra(tabCount = tabs.size))

        val data = tabs.map {
            ShareData(url = it.content.url, title = it.content.title)
        }
        val directions = TabsTrayFragmentDirections.actionGlobalShareFragment(
            data = data.toTypedArray(),
        )
        navController.navigate(directions)
    }

    override fun onShareTabsOfTypeClicked(private: Boolean) {
        val tabs = browserStore.state.getNormalOrPrivateTabs(private)
        val data = tabs.map {
            ShareData(url = it.content.url, title = it.content.title)
        }
        val directions = TabsTrayFragmentDirections.actionGlobalShareFragment(
            data = data.toTypedArray(),
        )
        navController.navigate(directions)
    }

    override fun onCloseAllTabsClicked(private: Boolean) {
        closeAllTabs(private, isConfirmed = false)
    }

    override fun onCloseAllPrivateTabsWarningConfirmed(private: Boolean) {
        closeAllTabs(private, isConfirmed = true)
    }

    private fun closeAllTabs(private: Boolean, isConfirmed: Boolean) {
        val sessionsToClose = if (private) {
            HomeFragment.ALL_PRIVATE_TABS
        } else {
            HomeFragment.ALL_NORMAL_TABS
        }

        if (private && !isConfirmed) {
            val privateDownloads = browserStore.state.downloads.filter {
                it.value.private && it.value.isActiveDownload()
            }
            if (privateDownloads.isNotEmpty()) {
                showCancelledDownloadWarning(privateDownloads.size, null, null)
                return
            }
        }
        dismissTabTrayAndNavigateHome(sessionsToClose)
    }

    override fun onSaveToCollections(tabs: Collection<TabSessionState>) {
        TabsTray.selectedTabsToCollection.record(TabsTray.SelectedTabsToCollectionExtra(tabCount = tabs.size))

        TabsTray.saveToCollection.record(NoExtras())
        tabsTrayStore.dispatch(TabsTrayAction.ExitSelectMode)

        CollectionsDialog(
            storage = collectionStorage,
            sessionList = browserStore.getTabSessionState(tabs),
            onPositiveButtonClick = { id, isNewCollection ->

                // If collection is null, a new one was created.
                if (isNewCollection) {
                    Collections.saved.record(
                        Collections.SavedExtra(
                            browserStore.state.normalTabs.size.toString(),
                            tabs.size.toString(),
                        ),
                    )
                } else {
                    Collections.tabsAdded.record(
                        Collections.TabsAddedExtra(
                            browserStore.state.normalTabs.size.toString(),
                            tabs.size.toString(),
                        ),
                    )
                }
                id?.apply {
                    showCollectionSnackbar(tabs.size, isNewCollection)
                }
            },
            onNegativeButtonClick = {},
        ).show(context)
    }

    override fun onSaveToBookmarks(tabs: Collection<TabSessionState>) {
        TabsTray.bookmarkSelectedTabs.record(TabsTray.BookmarkSelectedTabsExtra(tabCount = tabs.size))

        tabs.forEach { tab ->
            // We don't combine the context with lifecycleScope so that our jobs are not cancelled
            // if we leave the fragment, i.e. we still want the bookmarks to be added if the
            // tabs tray closes before the job is done.
            CoroutineScope(ioDispatcher).launch {
                bookmarksUseCase.addBookmark(tab.content.url, tab.content.title)
            }
        }

        showBookmarkSnackbar(tabs.size)
    }
}
