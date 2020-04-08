/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins

import android.content.Context
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.SimpleBrowserMenuHighlightableItem
import mozilla.components.support.ktx.android.content.getColorFromAttr
import org.mozilla.fenix.R
import org.mozilla.fenix.theme.ThemeManager

class SavedLoginsSortingStrategyMenu(
    private val context: Context,
    private val itemToHighlight: Item,
    private val onItemTapped: (Item) -> Unit = {}
) {
    sealed class Item {
        object AlphabeticallySort : Item()
        object LastUsedSort : Item()
    }

    val menuBuilder by lazy { BrowserMenuBuilder(menuItems) }

    private val menuItems by lazy {
        listOfNotNull(
            SimpleBrowserMenuHighlightableItem(
                label = context.getString(R.string.saved_logins_sort_strategy_alphabetically),
                textColorResource = ThemeManager.resolveAttribute(R.attr.primaryText, context),
                itemType = Item.AlphabeticallySort,
                backgroundTint = context.getColorFromAttr(R.attr.colorControlHighlight),
                isHighlighted = { itemToHighlight == Item.AlphabeticallySort }
            ) {
                onItemTapped.invoke(Item.AlphabeticallySort)
            },

            SimpleBrowserMenuHighlightableItem(
                label = context.getString(R.string.saved_logins_sort_strategy_last_used),
                textColorResource = ThemeManager.resolveAttribute(R.attr.primaryText, context),
                itemType = Item.LastUsedSort,
                backgroundTint = context.getColorFromAttr(R.attr.colorControlHighlight),
                isHighlighted = { itemToHighlight == Item.LastUsedSort }
            ) {
                onItemTapped.invoke(Item.LastUsedSort)
            }
        )
    }

    internal fun updateMenu(itemToHighlight: Item) {
        menuItems.forEach {
            it.isHighlighted = { itemToHighlight == it.itemType }
        }
    }
}
