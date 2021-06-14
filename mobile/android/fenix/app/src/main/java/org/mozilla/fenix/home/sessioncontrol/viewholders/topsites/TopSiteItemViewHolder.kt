/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.topsites

import android.annotation.SuppressLint
import android.content.Context
import android.view.MotionEvent
import android.view.View
import android.widget.PopupWindow
import androidx.appcompat.content.res.AppCompatResources.getDrawable
import kotlinx.android.synthetic.main.top_site_item.*
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.feature.top.sites.TopSite.Type.DEFAULT
import mozilla.components.feature.top.sites.TopSite.Type.FRECENT
import mozilla.components.feature.top.sites.TopSite.Type.PINNED
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.loadIntoView
import org.mozilla.fenix.home.sessioncontrol.TopSiteInteractor
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.utils.view.ViewHolder

class TopSiteItemViewHolder(
    view: View,
    private val interactor: TopSiteInteractor
) : ViewHolder(view) {
    private lateinit var topSite: TopSite

    init {
        top_site_item.setOnClickListener {
            interactor.onSelectTopSite(topSite.url, topSite.type)
        }

        top_site_item.setOnLongClickListener {
            interactor.onTopSiteMenuOpened()
            it.context.components.analytics.metrics.track(Event.TopSiteLongPress(topSite.type))

            val topSiteMenu = TopSiteItemMenu(view.context, topSite.type != FRECENT) { item ->
                when (item) {
                    is TopSiteItemMenu.Item.OpenInPrivateTab -> interactor.onOpenInPrivateTabClicked(
                        topSite
                    )
                    is TopSiteItemMenu.Item.RenameTopSite -> interactor.onRenameTopSiteClicked(
                        topSite
                    )
                    is TopSiteItemMenu.Item.RemoveTopSite -> interactor.onRemoveTopSiteClicked(
                        topSite
                    )
                }
            }
            val menu = topSiteMenu.menuBuilder.build(view.context).show(anchor = it)
            it.setOnTouchListener @SuppressLint("ClickableViewAccessibility") { v, event ->
                onTouchEvent(v, event, menu)
            }
            true
        }
    }

    fun bind(topSite: TopSite) {
        top_site_title.text = topSite.title

        if (topSite.type == PINNED || topSite.type == DEFAULT) {
            val pinIndicator = getDrawable(itemView.context, R.drawable.ic_new_pin)
            top_site_title.setCompoundDrawablesWithIntrinsicBounds(pinIndicator, null, null, null)
        } else {
            top_site_title.setCompoundDrawablesWithIntrinsicBounds(null, null, null, null)
        }

        when (topSite.url) {
            SupportUtils.POCKET_TRENDING_URL -> {
                favicon_image.setImageDrawable(getDrawable(itemView.context, R.drawable.ic_pocket))
            }
            SupportUtils.BAIDU_URL -> {
                favicon_image.setImageDrawable(getDrawable(itemView.context, R.drawable.ic_baidu))
            }
            SupportUtils.JD_URL -> {
                favicon_image.setImageDrawable(getDrawable(itemView.context, R.drawable.ic_jd))
            }
            SupportUtils.PDD_URL -> {
                favicon_image.setImageDrawable(getDrawable(itemView.context, R.drawable.ic_pdd))
            }
            else -> {
                itemView.context.components.core.icons.loadIntoView(favicon_image, topSite.url)
            }
        }

        this.topSite = topSite
    }

    private fun onTouchEvent(
        v: View,
        event: MotionEvent,
        menu: PopupWindow
    ): Boolean {
        if (event.action == MotionEvent.ACTION_CANCEL) {
            menu.dismiss()
        }
        return v.onTouchEvent(event)
    }

    companion object {
        const val LAYOUT_ID = R.layout.top_site_item
    }
}

class TopSiteItemMenu(
    private val context: Context,
    private val isPinnedSite: Boolean,
    private val onItemTapped: (Item) -> Unit = {}
) {
    sealed class Item {
        object OpenInPrivateTab : Item()
        object RenameTopSite : Item()
        object RemoveTopSite : Item()
    }

    val menuBuilder by lazy { BrowserMenuBuilder(menuItems) }

    private val menuItems by lazy {
        listOfNotNull(
            SimpleBrowserMenuItem(
                context.getString(R.string.bookmark_menu_open_in_private_tab_button)
            ) {
                onItemTapped.invoke(Item.OpenInPrivateTab)
            },
            if (isPinnedSite) SimpleBrowserMenuItem(
                context.getString(R.string.rename_top_site)
            ) {
                onItemTapped.invoke(Item.RenameTopSite)
            } else null,
            SimpleBrowserMenuItem(
                if (isPinnedSite) {
                    context.getString(R.string.remove_top_site)
                } else {
                    context.getString(R.string.delete_from_history)
                }
            ) {
                onItemTapped.invoke(Item.RemoveTopSite)
            }
        )
    }
}
