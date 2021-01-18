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

interface RecentlyClosedController {
    fun handleOpen(item: RecoverableTab, mode: BrowsingMode? = null)
    fun handleDeleteOne(tab: RecoverableTab)
    fun handleCopyUrl(item: RecoverableTab)
    fun handleShare(item: RecoverableTab)
    fun handleNavigateToHistory()
    fun handleRestore(item: RecoverableTab)
}

class DefaultRecentlyClosedController(
    private val navController: NavController,
    private val store: BrowserStore,
    private val tabsUseCases: TabsUseCases,
    private val resources: Resources,
    private val snackbar: FenixSnackbar,
    private val clipboardManager: ClipboardManager,
    private val activity: HomeActivity,
    private val openToBrowser: (item: RecoverableTab, mode: BrowsingMode?) -> Unit
) : RecentlyClosedController {
    override fun handleOpen(item: RecoverableTab, mode: BrowsingMode?) {
        openToBrowser(item, mode)
    }

    override fun handleDeleteOne(tab: RecoverableTab) {
        store.dispatch(RecentlyClosedAction.RemoveClosedTabAction(tab))
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

    override fun handleShare(item: RecoverableTab) {
        navController.navigate(
            RecentlyClosedFragmentDirections.actionGlobalShareFragment(
                data = arrayOf(ShareData(url = item.url, title = item.title))
            )
        )
    }

    override fun handleRestore(item: RecoverableTab) {
        tabsUseCases.restore(item)

        store.dispatch(
            RecentlyClosedAction.RemoveClosedTabAction(item)
        )

        activity.openToBrowser(
            from = BrowserDirection.FromRecentlyClosed
        )
    }
}
