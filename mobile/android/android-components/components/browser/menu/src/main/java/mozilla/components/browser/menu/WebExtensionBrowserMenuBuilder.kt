/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu

import android.content.Context
import mozilla.components.browser.menu.item.BackPressMenuItem
import mozilla.components.browser.menu.item.BrowserMenuDivider
import mozilla.components.browser.menu.item.BrowserMenuImageText
import mozilla.components.browser.menu.item.NO_ID
import mozilla.components.browser.menu.item.ParentBrowserMenuItem
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.store.BrowserStore

/**
 * Browser menu builder with web extension support. It allows [WebExtensionBrowserMenu] to add
 * web extension browser actions in a nested menu item. If there are no web extensions installed,
 * the web extension menu item would return an add-on manager menu item instead.
 *
 * @param store [BrowserStore] required to render web extension browser actions
 * @param webExtIconTintColorResource Optional ID of color resource to tint the icons of back and
 * add-ons manager menu items.
 * @param onAddonsManagerTapped Callback to be invoked when add-ons manager menu item is selected.
 * @param appendExtensionSubMenuAtStart true if web extension sub menu appear at the top (start) of
 * the menu, false if web extensions appear at the bottom of the menu. Default to false (bottom).
 */
@Suppress("LongParameterList")
class WebExtensionBrowserMenuBuilder(
    items: List<BrowserMenuItem>,
    extras: Map<String, Any> = emptyMap(),
    endOfMenuAlwaysVisible: Boolean = false,
    private val store: BrowserStore,
    private val webExtIconTintColorResource: Int = NO_ID,
    private val onAddonsManagerTapped: () -> Unit = {},
    private val appendExtensionSubMenuAtStart: Boolean = false
) : BrowserMenuBuilder(items, extras, endOfMenuAlwaysVisible) {

    /**
     * Builds and returns a browser menu with combination of [items] and web extension browser actions.
     */
    override fun build(context: Context): BrowserMenu {
        val extensionMenuItems =
            WebExtensionBrowserMenu.getOrUpdateWebExtensionMenuItems(store.state, store.state.selectedTab)

        val webExtMenuItem = if (extensionMenuItems.isNotEmpty()) {
            val backPressMenuItem = BackPressMenuItem(
                label = context.getString(R.string.mozac_browser_menu_addons),
                imageResource = R.drawable.mozac_ic_back,
                iconTintColorResource = webExtIconTintColorResource
            )

            val addonsManagerMenuItem = BrowserMenuImageText(
                label = context.getString(R.string.mozac_browser_menu_addons_manager),
                imageResource = R.drawable.mozac_ic_extensions,
                iconTintColorResource = webExtIconTintColorResource
            ) {
                onAddonsManagerTapped.invoke()
            }

            val webExtSubMenuItems = if (appendExtensionSubMenuAtStart) {
                listOf(backPressMenuItem) + BrowserMenuDivider() +
                extensionMenuItems +
                BrowserMenuDivider() + addonsManagerMenuItem
            } else {
                listOf(addonsManagerMenuItem) + BrowserMenuDivider() +
                extensionMenuItems +
                BrowserMenuDivider() + backPressMenuItem
            }

            val webExtBrowserMenuAdapter = BrowserMenuAdapter(context, webExtSubMenuItems)
            val webExtMenu = WebExtensionBrowserMenu(webExtBrowserMenuAdapter, store)

            ParentBrowserMenuItem(
                label = context.getString(R.string.mozac_browser_menu_addons),
                imageResource = R.drawable.mozac_ic_extensions,
                iconTintColorResource = webExtIconTintColorResource,
                subMenu = webExtMenu,
                endOfMenuAlwaysVisible = endOfMenuAlwaysVisible
            )
        } else {
            BrowserMenuImageText(
                label = context.getString(R.string.mozac_browser_menu_addons),
                imageResource = R.drawable.mozac_ic_extensions,
                iconTintColorResource = webExtIconTintColorResource
            ) {
                onAddonsManagerTapped.invoke()
            }
        }

        val menuItems =
            if (appendExtensionSubMenuAtStart) {
                listOf(webExtMenuItem) + items
            } else {
                items + webExtMenuItem
            }

        val adapter = BrowserMenuAdapter(context, menuItems)
        return BrowserMenu(adapter)
    }
}
