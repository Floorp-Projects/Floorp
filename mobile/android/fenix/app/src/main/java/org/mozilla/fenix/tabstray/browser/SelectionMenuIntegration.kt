/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.content.Context
import androidx.annotation.VisibleForTesting
import mozilla.components.browser.menu.BrowserMenuBuilder
import org.mozilla.fenix.tabstray.TabsTrayInteractor

class SelectionMenuIntegration(
    private val context: Context,
    private val interactor: TabsTrayInteractor,
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
        when (item) {
            is SelectionMenu.Item.BookmarkTabs -> {
                interactor.onBookmarkSelectedTabsClicked()
            }
            is SelectionMenu.Item.DeleteTabs -> {
                interactor.onDeleteSelectedTabsClicked()
            }
            is SelectionMenu.Item.MakeInactive -> {
                interactor.onForceSelectedTabsAsInactiveClicked()
            }
        }
    }
}
