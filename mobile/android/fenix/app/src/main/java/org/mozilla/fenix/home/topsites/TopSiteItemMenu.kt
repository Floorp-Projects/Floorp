/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.topsites

import android.content.Context
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import mozilla.components.feature.top.sites.TopSite
import org.mozilla.fenix.R

/**
 * Helper class for building a context menu for a top site item.
 *
 * @param context An Android context.
 * @param topSite The [TopSite] to show the context menu for.
 * @param onItemTapped Callback invoked when the user taps on a menu item.
 */
class TopSiteItemMenu(
    private val context: Context,
    private val topSite: TopSite,
    private val onItemTapped: (Item) -> Unit = {},
) {
    sealed class Item {
        object OpenInPrivateTab : Item()
        object RenameTopSite : Item()
        object RemoveTopSite : Item()
        object Settings : Item()
        object SponsorPrivacy : Item()
    }

    val menuBuilder by lazy { BrowserMenuBuilder(menuItems) }

    private val menuItems by lazy {
        val isPinnedSite = topSite is TopSite.Pinned || topSite is TopSite.Default
        val isProvidedSite = topSite is TopSite.Provided

        listOfNotNull(
            SimpleBrowserMenuItem(
                context.getString(R.string.bookmark_menu_open_in_private_tab_button),
            ) {
                onItemTapped.invoke(Item.OpenInPrivateTab)
            },
            if (isPinnedSite) {
                SimpleBrowserMenuItem(
                    context.getString(R.string.rename_top_site),
                ) {
                    onItemTapped.invoke(Item.RenameTopSite)
                }
            } else {
                null
            },
            if (!isProvidedSite) {
                SimpleBrowserMenuItem(
                    if (isPinnedSite) {
                        context.getString(R.string.remove_top_site)
                    } else {
                        context.getString(R.string.delete_from_history)
                    },
                ) {
                    onItemTapped.invoke(Item.RemoveTopSite)
                }
            } else {
                null
            },
            if (isProvidedSite) {
                SimpleBrowserMenuItem(
                    context.getString(R.string.top_sites_menu_settings),
                ) {
                    onItemTapped.invoke(Item.Settings)
                }
            } else {
                null
            },
            if (isProvidedSite) {
                SimpleBrowserMenuItem(
                    context.getString(R.string.top_sites_menu_sponsor_privacy),
                ) {
                    onItemTapped.invoke(Item.SponsorPrivacy)
                }
            } else {
                null
            },
        )
    }
}
