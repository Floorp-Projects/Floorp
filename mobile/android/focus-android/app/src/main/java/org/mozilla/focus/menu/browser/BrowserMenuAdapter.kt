/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.menu.browser

import android.app.PendingIntent
import android.content.Context
import android.support.v7.widget.RecyclerView
import android.view.LayoutInflater
import android.view.ViewGroup

import org.mozilla.focus.R
import org.mozilla.focus.customtabs.CustomTabConfig
import org.mozilla.focus.fragment.BrowserFragment
import org.mozilla.focus.utils.AppConstants
import org.mozilla.focus.utils.Browsers
import org.mozilla.focus.utils.HardwareUtils

import java.lang.ref.WeakReference

class BrowserMenuAdapter(
    private val context: Context,
    private val menu: BrowserMenu,
    private val fragment: BrowserFragment,
    customTabConfig: CustomTabConfig?
) : RecyclerView.Adapter<BrowserMenuViewHolder>() {
    internal data class MenuItem(val id: Int, val label: String, val pendingIntent: PendingIntent? = null)

    private var items = mutableListOf<MenuItem>()
    private var navigationItemViewHolderReference = WeakReference<NavigationItemViewHolder>(null)
    private var blockingItemViewHolderReference = WeakReference<BlockingItemViewHolder>(null)
    private var hasCustomTabConfig = customTabConfig != null

    private val blockingSwitchPosition: Int
        get() = if (shouldShowButtonToolbar()) 1 else 0

    private val requestDesktopCheckPosition: Int
        get() = if (hasCustomTabConfig) 4 else itemCount - 2

    init {
        initializeMenu(fragment.url, customTabConfig)
    }

    private fun initializeMenu(url: String, customTabConfig: CustomTabConfig?) {
        val resources = context.resources
        val browsers = Browsers(context, url)

        if (customTabConfig == null || customTabConfig.showShareMenuItem) {
            items.add(MenuItem(R.id.share, resources.getString(R.string.menu_share)))
        }

        items.add(MenuItem(R.id.add_to_homescreen, resources.getString(R.string.menu_add_to_home_screen)))

        if (browsers.hasMultipleThirdPartyBrowsers(context)) {
            items.add(MenuItem(R.id.open_select_browser, resources.getString(
                    R.string.menu_open_with_a_browser2)))
        }

        if (customTabConfig != null) {
            // "Open in Firefox Focus" to switch from a custom tab to the full-featured browser
            items.add(MenuItem(R.id.open_in_firefox_focus, resources.getString(R.string.menu_open_with_default_browser2,
                    resources.getString(R.string.app_name))))
        }

        if (browsers.hasThirdPartyDefaultBrowser(context)) {
            items.add(MenuItem(R.id.open_default, resources.getString(
                    R.string.menu_open_with_default_browser2, browsers.defaultBrowser!!.loadLabel(
                    context.packageManager))))
        }

        if (customTabConfig == null) {
            // There’s no need for Settings in a custom tab. The user can go to the browser app itself in order to do this.
            items.add(MenuItem(R.id.settings, resources.getString(R.string.menu_settings)))
        }

        if (AppConstants.isGeckoBuild()) {
            // "Report Site Issue" is available for builds using GeckoView only
            items.add(MenuItem(R.id.report_site_issue, resources.getString(R.string.menu_report_site_issue)))
        }

        if (customTabConfig != null) {
            addCustomTabMenuItems(items, customTabConfig)
        }
    }

    private fun addCustomTabMenuItems(items: MutableList<MenuItem>, customTabConfig: CustomTabConfig) {
        customTabConfig.menuItems
                .map { MenuItem(R.id.custom_tab_menu_item, it.name, it.pendingIntent) }
                .forEach { items.add(it) }
    }

    fun updateTrackers(trackers: Int) {
        val navigationItemViewHolder = blockingItemViewHolderReference.get() ?: return
        navigationItemViewHolder.updateTrackers(trackers)
    }

    fun updateLoading(loading: Boolean) {
        val navigationItemViewHolder = navigationItemViewHolderReference.get() ?: return
        navigationItemViewHolder.updateLoading(loading)
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): BrowserMenuViewHolder {
        val inflater = LayoutInflater.from(parent.context)

        return when (viewType) {
            NavigationItemViewHolder.LAYOUT_ID -> {
                val navigationItemViewHolder = NavigationItemViewHolder(
                        inflater.inflate(R.layout.menu_navigation, parent, false), fragment)
                navigationItemViewHolderReference = WeakReference(navigationItemViewHolder)
                navigationItemViewHolder
            }
            BlockingItemViewHolder.LAYOUT_ID -> {
                val blockingItemViewHolder = BlockingItemViewHolder(
                        inflater.inflate(R.layout.menu_blocking_switch, parent, false), fragment)
                blockingItemViewHolderReference = WeakReference(blockingItemViewHolder)
                blockingItemViewHolder
            }
            RequestDesktopCheckItemViewHolder.LAYOUT_ID -> {
                RequestDesktopCheckItemViewHolder(
                        inflater.inflate(R.layout.request_desktop_check_menu_item, parent, false),
                        fragment)
            }
            MenuItemViewHolder.LAYOUT_ID -> MenuItemViewHolder(inflater.inflate(R.layout.menu_item, parent, false))
            CustomTabMenuItemViewHolder.LAYOUT_ID -> CustomTabMenuItemViewHolder(inflater.inflate(R.layout.custom_tab_menu_item, parent, false))
            else -> throw IllegalArgumentException("Unknown view type: $viewType")
        }
    }

    private fun translateToMenuPosition(position: Int): Int {
        var offset = 1
        if (shouldShowButtonToolbar()) offset++
        if (position > requestDesktopCheckPosition) offset++
        return position - offset
    }

    override fun onBindViewHolder(holder: BrowserMenuViewHolder, position: Int) {
        holder.menu = menu
        holder.setOnClickListener(fragment)

        val actualPosition = translateToMenuPosition(position)

        if (actualPosition >= 0 && position != blockingSwitchPosition && position!= requestDesktopCheckPosition) {
            (holder as MenuItemViewHolder).bind(items[actualPosition])
        }
    }

    override fun getItemViewType(position: Int): Int {
        return if (position == 0 && shouldShowButtonToolbar()) {
            NavigationItemViewHolder.LAYOUT_ID
        } else if (position == blockingSwitchPosition) {
            BlockingItemViewHolder.LAYOUT_ID
        } else if (position == requestDesktopCheckPosition) {
            RequestDesktopCheckItemViewHolder.LAYOUT_ID
        } else {
            val actualPosition = translateToMenuPosition(position)
            val menuItem = items[actualPosition]

            if (menuItem.id == R.id.custom_tab_menu_item) {
                CustomTabMenuItemViewHolder.LAYOUT_ID
            } else {
                MenuItemViewHolder.LAYOUT_ID
            }
        }
    }

    override fun getItemCount(): Int {
        var itemCount = items.size

        if (shouldShowButtonToolbar()) {
            itemCount++
        }

        // For the blocking switch
        itemCount++

        // For the Request Desktop Check
        itemCount++

        return itemCount
    }

    // On phones we show an extra row with toolbar items (forward/refresh)
    private fun shouldShowButtonToolbar(): Boolean = !HardwareUtils.isTablet(context)
}
