/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu

import android.content.Context
import androidx.annotation.ColorRes
import androidx.annotation.DrawableRes
import mozilla.components.browser.menu.item.BackPressMenuItem
import mozilla.components.browser.menu.item.BrowserMenuDivider
import mozilla.components.browser.menu.item.BrowserMenuImageText
import mozilla.components.browser.menu.item.NO_ID
import mozilla.components.browser.menu.item.ParentBrowserMenuItem
import mozilla.components.browser.menu.item.WebExtensionBrowserMenuItem
import mozilla.components.browser.menu.item.WebExtensionPlaceholderMenuItem
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.store.BrowserStore

/**
 * Browser menu builder with web extension support. It allows [WebExtensionBrowserMenu] to add
 * web extension browser actions in a nested menu item. If there are no web extensions installed
 * and @param showAddonsInMenu is true the web extension menu item would return an add-on manager menu item instead.
 *
 * @param store [BrowserStore] required to render web extension browser actions
 * @param style Indicates how items should look like.
 * @param onAddonsManagerTapped Callback to be invoked when add-ons manager menu item is selected.
 * @param appendExtensionSubMenuAtStart Used when the menu does not have a [WebExtensionPlaceholderMenuItem]
 * to specify the place the extensions sub-menu should be inserted. True if web extension sub menu
 * appear at the top (start) of the menu, false if web extensions appear at the bottom of the menu.
 * Default to false (bottom). This is also used to decide the back press menu item placement at top or bottom.
 * @param showAddonsInMenu Whether to show the 'Add-ons' item in menu
 */
@Suppress("LongParameterList")
class WebExtensionBrowserMenuBuilder(
    items: List<BrowserMenuItem>,
    extras: Map<String, Any> = emptyMap(),
    endOfMenuAlwaysVisible: Boolean = false,
    private val store: BrowserStore,
    private val style: Style = Style(),
    private val onAddonsManagerTapped: () -> Unit = {},
    private val appendExtensionSubMenuAtStart: Boolean = false,
    private val showAddonsInMenu: Boolean = true,
) : BrowserMenuBuilder(items, extras, endOfMenuAlwaysVisible) {

    /**
     * Builds and returns a browser menu with combination of [items] and web extension browser actions.
     */
    override fun build(context: Context): BrowserMenu {
        val extensionMenuItems =
            WebExtensionBrowserMenu.getOrUpdateWebExtensionMenuItems(store.state, store.state.selectedTab)

        val finalList = items.toMutableList()

        val filteredExtensionMenuItems = extensionMenuItems.filter { webExtensionBrowserMenuItem ->
            replaceMenuPlaceholderWithExtensions(finalList, webExtensionBrowserMenuItem)
        }

        val menuItems = if (showAddonsInMenu) {
            createAddonsMenuItems(context, finalList, filteredExtensionMenuItems)
        } else {
            finalList
        }

        val adapter = BrowserMenuAdapter(context, menuItems)
        return BrowserMenu(adapter)
    }

    private fun replaceMenuPlaceholderWithExtensions(
        items: MutableList<BrowserMenuItem>,
        menuItem: WebExtensionBrowserMenuItem,
    ): Boolean {
        // Check if we have a placeholder
        val index = items.indexOfFirst { browserMenuItem ->
            (browserMenuItem as? WebExtensionPlaceholderMenuItem)?.id == menuItem.id
        }
        // Replace placeholder with corresponding web extension, and remove it from extensions menu list
        if (index != -1) {
            menuItem.setIconTint(
                (items[index] as? WebExtensionPlaceholderMenuItem)?.iconTintColorResource,
            )
            items[index] = menuItem
        }
        return index == -1
    }

    private fun createAddonsMenuItems(
        context: Context,
        items: MutableList<BrowserMenuItem>,
        filteredExtensionMenuItems: List<WebExtensionBrowserMenuItem>,
    ): List<BrowserMenuItem> {
        val addonsMenuItem = if (filteredExtensionMenuItems.isNotEmpty()) {
            val backPressMenuItem = BackPressMenuItem(
                label = context.getString(R.string.mozac_browser_menu_addons),
                imageResource = style.backPressMenuItemDrawableRes,
                iconTintColorResource = style.webExtIconTintColorResource,
            )

            val addonsManagerMenuItem = BrowserMenuImageText(
                label = context.getString(R.string.mozac_browser_menu_addons_manager),
                imageResource = style.addonsManagerMenuItemDrawableRes,
                iconTintColorResource = style.webExtIconTintColorResource,
            ) {
                onAddonsManagerTapped.invoke()
            }

            val webExtSubMenuItems = if (appendExtensionSubMenuAtStart) {
                listOf(backPressMenuItem) + BrowserMenuDivider() +
                    filteredExtensionMenuItems +
                    BrowserMenuDivider() + addonsManagerMenuItem
            } else {
                listOf(addonsManagerMenuItem) + BrowserMenuDivider() +
                    filteredExtensionMenuItems +
                    BrowserMenuDivider() + backPressMenuItem
            }

            val webExtBrowserMenuAdapter = BrowserMenuAdapter(context, webExtSubMenuItems)
            val webExtMenu = WebExtensionBrowserMenu(webExtBrowserMenuAdapter, store)

            ParentBrowserMenuItem(
                label = context.getString(R.string.mozac_browser_menu_addons),
                imageResource = style.addonsManagerMenuItemDrawableRes,
                iconTintColorResource = style.webExtIconTintColorResource,
                subMenu = webExtMenu,
                endOfMenuAlwaysVisible = endOfMenuAlwaysVisible,
            )
        } else {
            BrowserMenuImageText(
                label = context.getString(R.string.mozac_browser_menu_addons),
                imageResource = style.addonsManagerMenuItemDrawableRes,
                iconTintColorResource = style.webExtIconTintColorResource,
            ) {
                onAddonsManagerTapped.invoke()
            }
        }
        val mainMenuIndex = items.indexOfFirst { browserMenuItem ->
            (browserMenuItem as? WebExtensionPlaceholderMenuItem)?.id ==
                WebExtensionPlaceholderMenuItem.MAIN_EXTENSIONS_MENU_ID
        }

        return if (mainMenuIndex != -1) {
            items[mainMenuIndex] = addonsMenuItem
            items
            // if we do not have a placeholder we should add the extension submenu at top or bottom
        } else {
            if (appendExtensionSubMenuAtStart) {
                listOf(addonsMenuItem) + items
            } else {
                items + addonsMenuItem
            }
        }
    }

    /**
     * Allows to customize how items should look like.
     */
    data class Style(
        @ColorRes
        val webExtIconTintColorResource: Int = NO_ID,
        @DrawableRes
        val backPressMenuItemDrawableRes: Int = R.drawable.mozac_ic_back,
        @DrawableRes
        val addonsManagerMenuItemDrawableRes: Int = R.drawable.mozac_ic_extensions,
    )
}
