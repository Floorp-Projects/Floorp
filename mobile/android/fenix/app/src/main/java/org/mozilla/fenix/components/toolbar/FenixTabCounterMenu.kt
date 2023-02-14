/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import androidx.annotation.VisibleForTesting
import mozilla.components.concept.menu.candidate.DividerMenuCandidate
import mozilla.components.concept.menu.candidate.MenuCandidate
import mozilla.components.ui.tabcounter.TabCounterMenu
import org.mozilla.fenix.browser.browsingmode.BrowsingMode

class FenixTabCounterMenu(
    context: Context,
    onItemTapped: (Item) -> Unit,
    iconColor: Int? = null,
) : TabCounterMenu(context, onItemTapped, iconColor) {

    @VisibleForTesting
    internal fun menuItems(showOnly: BrowsingMode): List<MenuCandidate> {
        return when (showOnly) {
            BrowsingMode.Normal -> listOf(newTabItem)
            BrowsingMode.Private -> listOf(newPrivateTabItem)
        }
    }

    @VisibleForTesting
    internal fun menuItems(toolbarPosition: ToolbarPosition): List<MenuCandidate> {
        val items = listOf(
            newTabItem,
            newPrivateTabItem,
            DividerMenuCandidate(),
            closeTabItem,
        )

        return when (toolbarPosition) {
            ToolbarPosition.BOTTOM -> items.reversed()
            ToolbarPosition.TOP -> items
        }
    }

    /**
     * Update the displayed menu items.
     * @param showOnly Show only the new tab item corresponding to the given [BrowsingMode].
     */
    fun updateMenu(showOnly: BrowsingMode) {
        val items = menuItems(showOnly)

        menuController.submitList(items)
    }

    /**
     * Update the displayed menu items.
     * @param toolbarPosition Return a list that is ordered based on the given [ToolbarPosition].
     */
    fun updateMenu(toolbarPosition: ToolbarPosition) {
        menuController.submitList(menuItems(toolbarPosition))
    }
}
