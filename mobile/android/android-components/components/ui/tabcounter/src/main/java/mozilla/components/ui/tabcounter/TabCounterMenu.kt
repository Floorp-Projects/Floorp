/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.ui.tabcounter

import android.content.Context
import androidx.core.content.ContextCompat.getColor
import mozilla.components.browser.menu2.BrowserMenuController
import mozilla.components.concept.menu.MenuController
import mozilla.components.concept.menu.candidate.DrawableMenuIcon
import mozilla.components.concept.menu.candidate.TextMenuCandidate
import mozilla.components.concept.menu.candidate.TextStyle

/**
 * The menu that is shown when clicking on the [TabCounter]
 *
 * @param context the context.
 * @param onItemTapped behavior for when an item in the menu is tapped.
 * @param iconColor optional color to specify tint of menu icons
 */
open class TabCounterMenu(
    context: Context,
    onItemTapped: (Item) -> Unit,
    iconColor: Int? = null,
) {

    /**
     * Represents the menu items.
     *
     * [CloseTab] menu item for closing a tab.
     * [NewTab] menu item for opening a new tab.
     * [NewPrivateTab] menu item for opening a new private tab.
     * [DuplicateTab] menu item for duplicating the current tab.
     */
    @Suppress("UndocumentedPublicClass")
    open class Item {
        object CloseTab : Item()
        object NewTab : Item()
        object NewPrivateTab : Item()
        object DuplicateTab : Item()
    }

    var newTabItem: TextMenuCandidate
    var newPrivateTabItem: TextMenuCandidate
    var closeTabItem: TextMenuCandidate
    var duplicateTabItem: TextMenuCandidate

    val menuController: MenuController by lazy { BrowserMenuController() }

    init {
        newTabItem = TextMenuCandidate(
            text = context.getString(R.string.mozac_browser_menu_new_tab),
            start = DrawableMenuIcon(
                context,
                R.drawable.mozac_ic_new,
                tint = iconColor ?: getColor(context, R.color.mozac_ui_tabcounter_default_text),
            ),
            textStyle = TextStyle(),
        ) {
            onItemTapped(Item.NewTab)
        }

        newPrivateTabItem = TextMenuCandidate(
            text = context.getString(R.string.mozac_browser_menu_new_private_tab),
            start = DrawableMenuIcon(
                context,
                R.drawable.mozac_ic_private_browsing,
                tint = iconColor ?: getColor(context, R.color.mozac_ui_tabcounter_default_text),
            ),
            textStyle = TextStyle(),
        ) {
            onItemTapped(Item.NewPrivateTab)
        }

        closeTabItem = TextMenuCandidate(
            text = context.getString(R.string.mozac_close_tab),
            start = DrawableMenuIcon(
                context,
                R.drawable.mozac_ic_close,
                tint = iconColor ?: getColor(context, R.color.mozac_ui_tabcounter_default_text),
            ),
            textStyle = TextStyle(),
        ) {
            onItemTapped(Item.CloseTab)
        }

        duplicateTabItem = TextMenuCandidate(
            text = context.getString(R.string.mozac_ui_tabcounter_duplicate_tab),
            start = DrawableMenuIcon(
                context,
                R.drawable.mozac_ic_tab,
                tint = iconColor ?: getColor(context, R.color.mozac_ui_tabcounter_default_text),
            ),
            textStyle = TextStyle(),
        ) {
            onItemTapped(Item.DuplicateTab)
        }
    }
}
