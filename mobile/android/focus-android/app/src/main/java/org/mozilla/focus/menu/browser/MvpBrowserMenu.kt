/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.menu.browser

import android.content.Context
import androidx.core.content.ContextCompat
import mozilla.components.browser.menu.BrowserMenuHighlight
import mozilla.components.browser.menu.WebExtensionBrowserMenuBuilder
import mozilla.components.browser.menu.item.BrowserMenuDivider
import mozilla.components.browser.menu.item.BrowserMenuHighlightableItem
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
import org.mozilla.focus.theme.ThemeManager

/**
 * The overflow menu shown in the BrowserFragment containing page actions like "Refresh", "Share" etc.
 */
class MvpBrowserMenu(
    private val context: Context,
    private val store: BrowserStore,
    private val onItemTapped: (ToolbarMenu.Item) -> Unit = {},
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
            primaryImageTintResource = ThemeManager.resolveAttribute(R.attr.primaryText, context),
            isInPrimaryState = {
                selectedSession?.content?.canGoBack ?: true
            },
            secondaryImageTintResource = ThemeManager.resolveAttribute(R.attr.disabled, context),
            disableInSecondaryState = true,
            longClickListener = { onItemTapped.invoke(ToolbarMenu.Item.Back) }
        ) {
            onItemTapped.invoke(ToolbarMenu.Item.Back)
        }

        val forward = BrowserMenuItemToolbar.TwoStateButton(
            primaryImageResource = R.drawable.ic_forward,
            primaryContentDescription = context.getString(R.string.content_description_forward),
            primaryImageTintResource = ThemeManager.resolveAttribute(R.attr.primaryText, context),
            isInPrimaryState = {
                selectedSession?.content?.canGoForward ?: true
            },
            secondaryImageTintResource = ThemeManager.resolveAttribute(R.attr.disabled, context),
            disableInSecondaryState = true,
            longClickListener = { onItemTapped.invoke(ToolbarMenu.Item.Forward) }
        ) {
            onItemTapped.invoke(ToolbarMenu.Item.Forward)
        }

        val refresh = BrowserMenuItemToolbar.TwoStateButton(
            primaryImageResource = R.drawable.ic_refresh,
            primaryContentDescription = context.getString(R.string.content_description_reload),
            primaryImageTintResource = ThemeManager.resolveAttribute(R.attr.primaryText, context),
            isInPrimaryState = {
                selectedSession?.content?.loading == false
            },
            secondaryImageResource = R.drawable.ic_stop,
            secondaryContentDescription = context.getString(R.string.content_description_stop),
            secondaryImageTintResource = ThemeManager.resolveAttribute(R.attr.primaryText, context),
            disableInSecondaryState = false,
            longClickListener = { onItemTapped.invoke(ToolbarMenu.Item.Reload(bypassCache = true)) }
        ) {
            if (selectedSession?.content?.loading == true) {
                onItemTapped.invoke(ToolbarMenu.Item.Stop)
            } else {
                onItemTapped.invoke(ToolbarMenu.Item.Reload(bypassCache = false))
            }
        }
        val share = BrowserMenuItemToolbar.Button(
            imageResource = R.drawable.ic_mvp_share,
            contentDescription = context.getString(R.string.menu_share),
            listener = {
                onItemTapped.invoke(ToolbarMenu.Item.Share)
            }
        )
        BrowserMenuItemToolbar(listOf(back, forward, refresh, share))
    }

    val mvpMenuItems by lazy {
        val findInPage = BrowserMenuImageText(
            label = context.getString(R.string.find_in_page),
            imageResource = R.drawable.ic_search,
        ) {
            onItemTapped.invoke(ToolbarMenu.Item.FindInPage)
        }

        val desktopMode = BrowserMenuImageSwitch(
            imageResource = R.drawable.ic_device_desktop,
            label = context.getString(R.string.mvp_preference_performance_request_desktop_site),
            initialState = {
                selectedSession?.content?.desktopMode ?: false
            }
        ) { checked ->
            onItemTapped.invoke(ToolbarMenu.Item.RequestDesktop(checked))
        }

        val reportSiteIssuePlaceholder = WebExtensionPlaceholderMenuItem(
            id = WebCompatReporterFeature.WEBCOMPAT_REPORTER_EXTENSION_ID,
            iconTintColorResource = ThemeManager.resolveAttribute(R.attr.primaryText, context)
        )

        val addToHomescreen = BrowserMenuImageText(
            label = context.getString(R.string.menu_add_to_home_screen),
            imageResource = R.drawable.ic_add_to_home_screen,
        ) {
            onItemTapped.invoke(ToolbarMenu.Item.AddToHomeScreen)
        }

        val openInApp = BrowserMenuHighlightableItem(
            label = context.getString(R.string.menu_open_with_a_browser2),
            startImageResource = R.drawable.ic_help,
            textColorResource = ThemeManager.resolveAttribute(R.attr.primaryText, context),
            highlight = BrowserMenuHighlight.HighPriority(
                backgroundTint = ContextCompat.getColor(context, R.color.mvp_browser_menu_bg),
                canPropagate = false
            )
        ) {
            onItemTapped.invoke(ToolbarMenu.Item.OpenInApp)
        }

        val settings = BrowserMenuHighlightableItem(
            label = context.getString(R.string.menu_settings),
            startImageResource = R.drawable.ic_mvp_settings,
            textColorResource = ThemeManager.resolveAttribute(R.attr.primaryText, context),
            highlight = BrowserMenuHighlight.HighPriority(
                backgroundTint = ContextCompat.getColor(context, R.color.mvp_browser_menu_bg),
                canPropagate = false
            )
        ) {
            onItemTapped.invoke(ToolbarMenu.Item.Settings)
        }
        listOfNotNull(
            menuToolbar,
            BrowserMenuDivider(),
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
