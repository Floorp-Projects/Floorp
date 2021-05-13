/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.content.Context
import androidx.annotation.VisibleForTesting
import mozilla.components.browser.menu.BrowserMenuBuilder
import org.mozilla.fenix.tabstray.NavigationInteractor
import org.mozilla.fenix.tabstray.TabsTrayAction
import org.mozilla.fenix.tabstray.TabsTrayInteractor
import org.mozilla.fenix.tabstray.TabsTrayStore
import org.mozilla.fenix.utils.Do

class SelectionMenuIntegration(
    private val context: Context,
    private val store: TabsTrayStore,
    private val navInteractor: NavigationInteractor,
    private val trayInteractor: TabsTrayInteractor
) {
    private val menu by lazy {
        SelectionMenu(context, ::handleMenuClicked)
    }

    /**
     * Builds the internal menu items list. See [BrowserMenuBuilder.build].
     */
    fun build() = menu.menuBuilder.build(context)

    @VisibleForTesting
    internal fun handleMenuClicked(item: SelectionMenu.Item) {
        Do exhaustive when (item) {
            is SelectionMenu.Item.BookmarkTabs -> {
                navInteractor.onSaveToBookmarks(store.state.mode.selectedTabs)
                store.dispatch(TabsTrayAction.ExitSelectMode)
            }
            is SelectionMenu.Item.DeleteTabs -> {
                trayInteractor.onDeleteTabs(store.state.mode.selectedTabs)
                store.dispatch(TabsTrayAction.ExitSelectMode)
            }
        }
    }
}
