/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.topsites

import android.content.Context
import android.view.View
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.top_site_item.view.*
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.loadIntoView
import org.mozilla.fenix.home.sessioncontrol.TopSiteInteractor

class TopSiteItemViewHolder(
    private val view: View,
    private val interactor: TopSiteInteractor
) : RecyclerView.ViewHolder(view) {
    private lateinit var topSite: TopSite
    private var topSiteMenu: TopSiteItemMenu

    init {
        topSiteMenu = TopSiteItemMenu(view.context) {
            when (it) {
                is TopSiteItemMenu.Item.OpenInPrivateTab -> interactor.onOpenInPrivateTabClicked(
                    topSite
                )
                is TopSiteItemMenu.Item.RemoveTopSite -> interactor.onRemoveTopSiteClicked(topSite)
            }
        }

        view.top_site_item.setOnClickListener {
            interactor.onSelectTopSite(topSite.url)
        }

        view.top_site_item.setOnLongClickListener() {
            topSiteMenu.menuBuilder.build(view.context).show(anchor = it)
            return@setOnLongClickListener true
        }
    }

    fun bind(topSite: TopSite) {
        this.topSite = topSite
        view.top_site_title.text = topSite.title
        view.context.components.core.icons.loadIntoView(view.favicon_image, topSite.url)
    }

    companion object {
        const val LAYOUT_ID = R.layout.top_site_item
    }
}

class TopSiteItemMenu(
    private val context: Context,
    private val onItemTapped: (Item) -> Unit = {}
) {
    sealed class Item {
        object OpenInPrivateTab : Item()
        object RemoveTopSite : Item()
    }

    val menuBuilder by lazy { BrowserMenuBuilder(menuItems) }

    private val menuItems by lazy {
        listOf(
            SimpleBrowserMenuItem(
                context.getString(R.string.bookmark_menu_open_in_private_tab_button)
            ) {
                onItemTapped.invoke(Item.OpenInPrivateTab)
            },

            SimpleBrowserMenuItem(
                context.getString(R.string.remove_top_site)
            ) {
                onItemTapped.invoke(Item.RemoveTopSite)
            }
        )
    }
}
