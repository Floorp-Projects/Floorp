/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import androidx.navigation.NavController
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.selector.findCustomTabOrSelectedTab
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.selector.getNormalOrPrivateTabs
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.service.glean.private.NoExtras
import mozilla.components.support.ktx.kotlin.isUrl
import mozilla.components.ui.tabcounter.TabCounterMenu
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.GleanMetrics.ReaderMode
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserAnimator
import org.mozilla.fenix.browser.BrowserAnimator.Companion.getToolbarNavOptions
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.readermode.ReaderModeController
import org.mozilla.fenix.components.toolbar.interactor.BrowserToolbarInteractor
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.navigateSafe
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.home.HomeFragment
import org.mozilla.fenix.home.HomeScreenViewModel

/**
 * An interface that handles the view manipulation of the BrowserToolbar, triggered by the Interactor
 */
interface BrowserToolbarController {
    fun handleScroll(offset: Int)
    fun handleToolbarPaste(text: String)
    fun handleToolbarPasteAndGo(text: String)
    fun handleToolbarClick()
    fun handleTabCounterClick()
    fun handleTabCounterItemInteraction(item: TabCounterMenu.Item)
    fun handleReaderModePressed(enabled: Boolean)

    /**
     * @see [BrowserToolbarInteractor.onHomeButtonClicked]
     */
    fun handleHomeButtonClick()

    /**
     * @see [BrowserToolbarInteractor.onEraseButtonClicked]
     */
    fun handleEraseButtonClick()

    /**
     * @see [BrowserToolbarInteractor.onShoppingCfrActionClicked]
     */
    fun handleShoppingCfrActionClick()

    /**
     * @see [BrowserToolbarInteractor.onShoppingCfrDisplayed]
     */
    fun handleShoppingCfrDisplayed()

    /**
     * @see [BrowserToolbarInteractor.onTranslationsButtonClicked]
     */
    fun handleTranslationsButtonClick()

    /**
     * @see [BrowserToolbarInteractor.onShareActionClicked]
     */
    fun onShareActionClicked()
}

private const val MAX_DISPLAY_NUMBER_SHOPPING_CFR = 3

@Suppress("LongParameterList")
class DefaultBrowserToolbarController(
    private val store: BrowserStore,
    private val tabsUseCases: TabsUseCases,
    private val activity: HomeActivity,
    private val navController: NavController,
    private val readerModeController: ReaderModeController,
    private val engineView: EngineView,
    private val homeViewModel: HomeScreenViewModel,
    private val customTabSessionId: String?,
    private val browserAnimator: BrowserAnimator,
    private val onTabCounterClicked: () -> Unit,
    private val onCloseTab: (SessionState) -> Unit,
) : BrowserToolbarController {

    private val currentSession
        get() = store.state.findCustomTabOrSelectedTab(customTabSessionId)

    override fun handleToolbarPaste(text: String) {
        navController.nav(
            R.id.browserFragment,
            BrowserFragmentDirections.actionGlobalSearchDialog(
                sessionId = currentSession?.id,
                pastedText = text,
            ),
            getToolbarNavOptions(activity),
        )
    }

    override fun handleToolbarPasteAndGo(text: String) {
        if (text.isUrl()) {
            store.updateSearchTermsOfSelectedSession("")
            activity.components.useCases.sessionUseCases.loadUrl.invoke(text)
            return
        }

        store.updateSearchTermsOfSelectedSession(text)
        activity.components.useCases.searchUseCases.defaultSearch.invoke(
            text,
            sessionId = store.state.selectedTabId,
        )
    }

    override fun handleToolbarClick() {
        Events.searchBarTapped.record(Events.SearchBarTappedExtra("BROWSER"))
        // If we're displaying awesomebar search results, Home screen will not be visible (it's
        // covered up with the search results). So, skip the navigation event in that case.
        // If we don't, there's a visual flickr as we navigate to Home and then display search
        // results on top it.
        if (currentSession?.content?.searchTerms.isNullOrBlank()) {
            browserAnimator.captureEngineViewAndDrawStatically {
                navController.navigate(
                    BrowserFragmentDirections.actionGlobalHome(),
                )
                navController.navigate(
                    BrowserFragmentDirections.actionGlobalSearchDialog(
                        currentSession?.id,
                    ),
                    getToolbarNavOptions(activity),
                )
            }
        } else {
            navController.navigate(
                BrowserFragmentDirections.actionGlobalSearchDialog(
                    currentSession?.id,
                ),
                getToolbarNavOptions(activity),
            )
        }
    }

    override fun handleTabCounterClick() {
        onTabCounterClicked.invoke()
    }

    override fun handleReaderModePressed(enabled: Boolean) {
        if (enabled) {
            readerModeController.showReaderView()
            ReaderMode.opened.record(NoExtras())
        } else {
            readerModeController.hideReaderView()
            ReaderMode.closed.record(NoExtras())
        }
    }

    override fun handleTabCounterItemInteraction(item: TabCounterMenu.Item) {
        when (item) {
            is TabCounterMenu.Item.CloseTab -> {
                store.state.selectedTab?.let {
                    // When closing the last tab we must show the undo snackbar in the home fragment
                    if (store.state.getNormalOrPrivateTabs(it.content.private).count() == 1) {
                        homeViewModel.sessionToDelete = it.id
                        navController.navigate(
                            BrowserFragmentDirections.actionGlobalHome(),
                        )
                    } else {
                        onCloseTab.invoke(it)
                        tabsUseCases.removeTab(it.id, selectParentIfExists = true)
                    }
                }
            }
            is TabCounterMenu.Item.NewTab -> {
                activity.browsingModeManager.mode = BrowsingMode.Normal
                navController.navigate(
                    BrowserFragmentDirections.actionGlobalHome(focusOnAddressBar = true),
                )
            }
            is TabCounterMenu.Item.NewPrivateTab -> {
                activity.browsingModeManager.mode = BrowsingMode.Private
                navController.navigate(
                    BrowserFragmentDirections.actionGlobalHome(focusOnAddressBar = true),
                )
            }
        }
    }

    override fun handleScroll(offset: Int) {
        if (activity.settings().isDynamicToolbarEnabled) {
            engineView.setVerticalClipping(offset)
        }
    }

    override fun handleHomeButtonClick() {
        Events.browserToolbarHomeTapped.record(NoExtras())
        browserAnimator.captureEngineViewAndDrawStatically {
            navController.navigate(
                BrowserFragmentDirections.actionGlobalHome(),
            )
        }
    }

    override fun handleEraseButtonClick() {
        Events.browserToolbarEraseTapped.record(NoExtras())
        homeViewModel.sessionToDelete = HomeFragment.ALL_PRIVATE_TABS
        val directions = BrowserFragmentDirections.actionGlobalHome()
        navController.navigate(directions)
    }

    override fun handleShoppingCfrActionClick() {
        navController.navigate(
            BrowserFragmentDirections.actionBrowserFragmentToReviewQualityCheckDialogFragment(),
        )
    }

    override fun handleShoppingCfrDisplayed() {
        updateShoppingCfrSettings()
    }

    override fun handleTranslationsButtonClick() {
        val directions =
            BrowserFragmentDirections.actionBrowserFragmentToTranslationsDialogFragment(
                sessionId = currentSession?.id,
            )
        navController.navigateSafe(R.id.browserFragment, directions)
    }

    override fun onShareActionClicked() {
        val directions = NavGraphDirections.actionGlobalShareFragment(
            sessionId = currentSession?.id,
            data = arrayOf(
                ShareData(
                    url = getProperUrl(currentSession),
                    title = currentSession?.content?.title,
                ),
            ),
            showPage = true,
        )
        navController.navigate(directions)
    }

    private fun getProperUrl(currentSession: SessionState?): String? {
        return currentSession?.id?.let {
            val currentTab = store.state.findTab(it)
            if (currentTab?.readerState?.active == true) {
                currentTab.readerState.activeUrl
            } else {
                currentSession.content.url
            }
        }
    }

    companion object {
        internal const val TELEMETRY_BROWSER_IDENTIFIER = "browserMenu"
    }

    /**
     * Stop showing the CFR after being displayed three times with
     * with at least 12 hrs in-between.
     * As described in: https://bugzilla.mozilla.org/show_bug.cgi?id=1861173#c0
     */
    private fun updateShoppingCfrSettings() = with(activity.settings()) {
        reviewQualityCheckCFRClosedCounter++
        if (reviewQualityCheckCfrDisplayTimeInMillis != 0L &&
            reviewQualityCheckCFRClosedCounter >= MAX_DISPLAY_NUMBER_SHOPPING_CFR
        ) {
            shouldShowReviewQualityCheckCFR = false
        } else {
            reviewQualityCheckCfrDisplayTimeInMillis = System.currentTimeMillis()
        }
    }
}

private fun BrowserStore.updateSearchTermsOfSelectedSession(
    searchTerms: String,
) {
    val selectedTabId = state.selectedTabId ?: return

    dispatch(
        ContentAction.UpdateSearchTermsAction(
            selectedTabId,
            searchTerms,
        ),
    )
}
