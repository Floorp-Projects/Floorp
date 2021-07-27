/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.menu.browser

import android.content.Context
import mozilla.components.browser.menu.WebExtensionBrowserMenuBuilder
import mozilla.components.browser.menu.item.BrowserMenuDivider
import mozilla.components.browser.menu.item.BrowserMenuImageSwitch
import mozilla.components.browser.menu.item.BrowserMenuImageText
import mozilla.components.browser.menu.item.BrowserMenuItemToolbar
import mozilla.components.browser.menu.item.WebExtensionPlaceholderMenuItem
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.webcompat.reporter.WebCompatReporterFeature
import org.mozilla.focus.R
import org.mozilla.focus.menu.ToolbarMenu
import org.mozilla.focus.state.AppStore
import org.mozilla.focus.theme.resolveAttribute
import org.mozilla.focus.topsites.DefaultTopSitesStorage.Companion.TOP_SITES_MAX_LIMIT

/**
 * The overflow menu shown in the BrowserFragment containing page actions like "Refresh", "Share" etc.
 */
class DefaultBrowserMenu(
    private val context: Context,
    private val appStore: AppStore,
    private val store: BrowserStore,
    private val onItemTapped: (ToolbarMenu.Item) -> Unit = {}
) : ToolbarMenu {

    private val selectedSession: TabSessionState?
        get() = store.state.selectedTab

    override val menuBuilder by lazy {
        WebExtensionBrowserMenuBuilder(
            items = mvpMenuItems, store = store, showAddonsInMenu = false
        )
    }

    override val menuToolbar by lazy {
        val back = BrowserMenuItemToolbar.TwoStateButton(
            primaryImageResource = R.drawable.ic_back,
            primaryContentDescription = context.getString(R.string.content_description_back),
            primaryImageTintResource = context.theme.resolveAttribute(R.attr.primaryText),
            isInPrimaryState = {
                selectedSession?.content?.canGoBack ?: true
            },
            secondaryImageTintResource = context.theme.resolveAttribute(R.attr.disabled),
            disableInSecondaryState = true,
            longClickListener = { onItemTapped.invoke(ToolbarMenu.Item.Back) }
        ) {
            onItemTapped.invoke(ToolbarMenu.Item.Back)
        }

        val forward = BrowserMenuItemToolbar.TwoStateButton(
            primaryImageResource = R.drawable.ic_forward,
            primaryContentDescription = context.getString(R.string.content_description_forward),
            primaryImageTintResource = context.theme.resolveAttribute(R.attr.primaryText),
            isInPrimaryState = {
                selectedSession?.content?.canGoForward ?: true
            },
            secondaryImageTintResource = context.theme.resolveAttribute(R.attr.disabled),
            disableInSecondaryState = true,
            longClickListener = { onItemTapped.invoke(ToolbarMenu.Item.Forward) }
        ) {
            onItemTapped.invoke(ToolbarMenu.Item.Forward)
        }

        val refresh = BrowserMenuItemToolbar.TwoStateButton(
            primaryImageResource = R.drawable.ic_refresh,
            primaryContentDescription = context.getString(R.string.content_description_reload),
            primaryImageTintResource = context.theme.resolveAttribute(R.attr.primaryText),
            isInPrimaryState = {
                selectedSession?.content?.loading == false
            },
            secondaryImageResource = R.drawable.ic_stop,
            secondaryContentDescription = context.getString(R.string.content_description_stop),
            secondaryImageTintResource = context.theme.resolveAttribute(R.attr.primaryText),
            disableInSecondaryState = false,
            longClickListener = { onItemTapped.invoke(ToolbarMenu.Item.Reload) }
        ) {
            if (selectedSession?.content?.loading == true) {
                onItemTapped.invoke(ToolbarMenu.Item.Stop)
            } else {
                onItemTapped.invoke(ToolbarMenu.Item.Reload)
            }
        }
        val share = BrowserMenuItemToolbar.Button(
            imageResource = R.drawable.ic_share2,
            contentDescription = context.getString(R.string.menu_share),
            listener = {
                onItemTapped.invoke(ToolbarMenu.Item.Share)
            }
        )
        BrowserMenuItemToolbar(listOf(back, forward, refresh, share))
    }

    private val mvpMenuItems by lazy {
        val url = selectedSession?.content?.url
        val topSites = appStore.state.topSites
        val hasTopSite = topSites.find { it.url == url } != null
        val canAddTopSite = url != null && !hasTopSite && topSites.size < TOP_SITES_MAX_LIMIT

        val addToShortcuts = BrowserMenuImageText(
            label = context.getString(R.string.menu_add_to_shortcuts),
            imageResource = R.drawable.mozac_ic_pin
        ) {
            onItemTapped.invoke(ToolbarMenu.Item.AddToShortcuts)
        }

        val removeFromShortcuts = BrowserMenuImageText(
            label = context.getString(R.string.menu_remove_from_shortcuts),
            imageResource = R.drawable.mozac_ic_pin_remove
        ) {
            onItemTapped.invoke(ToolbarMenu.Item.RemoveFromShortcuts)
        }

        val findInPage = BrowserMenuImageText(
            label = context.getString(R.string.find_in_page),
            imageResource = R.drawable.ic_search
        ) {
            onItemTapped.invoke(ToolbarMenu.Item.FindInPage)
        }

        val desktopMode = BrowserMenuImageSwitch(
            imageResource = R.drawable.ic_device_desktop,
            label = context.getString(R.string.preference_performance_request_desktop_site2),
            initialState = {
                selectedSession?.content?.desktopMode ?: false
            }
        ) { checked ->
            onItemTapped.invoke(ToolbarMenu.Item.RequestDesktop(checked))
        }

        val reportSiteIssuePlaceholder = WebExtensionPlaceholderMenuItem(
            id = WebCompatReporterFeature.WEBCOMPAT_REPORTER_EXTENSION_ID,
            iconTintColorResource = context.theme.resolveAttribute(R.attr.primaryText)
        )

        val addToHomescreen = BrowserMenuImageText(
            label = context.getString(R.string.menu_add_to_home_screen),
            imageResource = R.drawable.ic_add_to_home_screen
        ) {
            onItemTapped.invoke(ToolbarMenu.Item.AddToHomeScreen)
        }

        val openInApp = BrowserMenuImageText(
            label = context.getString(R.string.menu_open_with_a_browser2),
            imageResource = R.drawable.ic_open_in,
            textColorResource = context.theme.resolveAttribute(R.attr.primaryText)
        ) {
            onItemTapped.invoke(ToolbarMenu.Item.OpenInApp)
        }

        val settings = BrowserMenuImageText(
            label = context.getString(R.string.menu_settings),
            imageResource = R.drawable.ic_settings2,
            textColorResource = context.theme.resolveAttribute(R.attr.primaryText)
        ) {
            onItemTapped.invoke(ToolbarMenu.Item.Settings)
        }

        listOfNotNull(
            menuToolbar,
            BrowserMenuDivider(),
            if (canAddTopSite) addToShortcuts else null,
            if (hasTopSite) removeFromShortcuts else null,
            if (canAddTopSite || hasTopSite) BrowserMenuDivider() else null,
            findInPage,
            desktopMode,
            reportSiteIssuePlaceholder,
            BrowserMenuDivider(),
            addToHomescreen,
            openInApp,
            BrowserMenuDivider(),
            settings
        )
    }
}
