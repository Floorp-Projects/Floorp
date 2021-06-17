/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.recentlyclosed

import android.content.ClipData
import android.content.ClipboardManager
import android.content.res.Resources
import androidx.navigation.NavController
import androidx.navigation.NavOptions
import mozilla.components.browser.state.action.RecentlyClosedAction
import mozilla.components.browser.state.state.recover.RecoverableTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.feature.tabs.TabsUseCases
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.FenixSnackbar

@Suppress("TooManyFunctions")
interface RecentlyClosedController {
    fun handleOpen(tab: RecoverableTab, mode: BrowsingMode? = null)
    fun handleOpen(tabs: Set<RecoverableTab>, mode: BrowsingMode? = null)
    fun handleDelete(tab: RecoverableTab)
    fun handleDelete(tabs: Set<RecoverableTab>)
    fun handleCopyUrl(item: RecoverableTab)
    fun handleShare(tab: RecoverableTab)
    fun handleShare(tabs: Set<RecoverableTab>)
    fun handleNavigateToHistory()
    fun handleRestore(item: RecoverableTab)
    fun handleSelect(tab: RecoverableTab)
    fun handleDeselect(tab: RecoverableTab)
    fun handleBackPressed(): Boolean
}

@Suppress("TooManyFunctions")
class DefaultRecentlyClosedController(
    private val navController: NavController,
    private val browserStore: BrowserStore,
    private val recentlyClosedStore: RecentlyClosedFragmentStore,
    private val tabsUseCases: TabsUseCases,
    private val resources: Resources,
    private val snackbar: FenixSnackbar,
    private val clipboardManager: ClipboardManager,
    private val activity: HomeActivity,
    private val openToBrowser: (item: RecoverableTab, mode: BrowsingMode?) -> Unit
) : RecentlyClosedController {
    override fun handleOpen(tab: RecoverableTab, mode: BrowsingMode?) {
        openToBrowser(tab, mode)
    }

    override fun handleOpen(tabs: Set<RecoverableTab>, mode: BrowsingMode?) {
        recentlyClosedStore.dispatch(RecentlyClosedFragmentAction.DeselectAll)
        tabs.forEach { tab -> handleOpen(tab, mode) }
    }

    override fun handleSelect(tab: RecoverableTab) {
        recentlyClosedStore.dispatch(RecentlyClosedFragmentAction.Select(tab))
    }

    override fun handleDeselect(tab: RecoverableTab) {
        recentlyClosedStore.dispatch(RecentlyClosedFragmentAction.Deselect(tab))
    }

    override fun handleDelete(tab: RecoverableTab) {
        browserStore.dispatch(RecentlyClosedAction.RemoveClosedTabAction(tab))
    }

    override fun handleDelete(tabs: Set<RecoverableTab>) {
        recentlyClosedStore.dispatch(RecentlyClosedFragmentAction.DeselectAll)
        tabs.forEach { tab -> handleDelete(tab) }
    }

    override fun handleNavigateToHistory() {
        navController.navigate(
            RecentlyClosedFragmentDirections.actionGlobalHistoryFragment(),
            NavOptions.Builder().setPopUpTo(R.id.historyFragment, true).build()
        )
    }

    override fun handleCopyUrl(item: RecoverableTab) {
        val urlClipData = ClipData.newPlainText(item.url, item.url)
        clipboardManager.setPrimaryClip(urlClipData)
        with(snackbar) {
            setText(resources.getString(R.string.url_copied))
            show()
        }
    }

    override fun handleShare(tab: RecoverableTab) = handleShare(setOf(tab))

    override fun handleShare(tabs: Set<RecoverableTab>) {
        val shareData = tabs.map { ShareData(url = it.url, title = it.title) }
        navController.navigate(
            RecentlyClosedFragmentDirections.actionGlobalShareFragment(
                data = shareData.toTypedArray()
            )
        )
    }

    override fun handleRestore(item: RecoverableTab) {
        tabsUseCases.restore(item)

        browserStore.dispatch(
            RecentlyClosedAction.RemoveClosedTabAction(item)
        )

        activity.openToBrowser(
            from = BrowserDirection.FromRecentlyClosed
        )
    }

    override fun handleBackPressed(): Boolean {
        return if (recentlyClosedStore.state.selectedTabs.isNotEmpty()) {
            recentlyClosedStore.dispatch(RecentlyClosedFragmentAction.DeselectAll)
            true
        } else {
            false
        }
    }
}
