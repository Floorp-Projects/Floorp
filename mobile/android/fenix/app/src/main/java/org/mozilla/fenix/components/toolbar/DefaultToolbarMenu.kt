/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.BrowserMenuDivider
import mozilla.components.browser.menu.item.BrowserMenuHighlightableItem
import mozilla.components.browser.menu.item.BrowserMenuImageText
import mozilla.components.browser.menu.item.BrowserMenuItemToolbar
import mozilla.components.browser.menu.item.BrowserMenuImageSwitch
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.storage.BookmarksStorage
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.asActivity
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.theme.ThemeManager
import org.mozilla.fenix.utils.Settings

@Suppress("LargeClass") // While large, most of the class is very simple
class DefaultToolbarMenu(
    private val context: Context,
    private val hasAccountProblem: Boolean = false,
    private val requestDesktopStateProvider: () -> Boolean = { false },
    private val onItemTapped: (ToolbarMenu.Item) -> Unit = {},
    private val lifecycleOwner: LifecycleOwner,
    private val bookmarksStorage: BookmarksStorage,
    readerModeStateProvider: () -> Boolean = { false },
    sessionManager: SessionManager
) : ToolbarMenu {

    private var currentUrlIsBookmarked = false
    private var isBookmarkedJob: Job? = null

    override val menuBuilder by lazy { BrowserMenuBuilder(menuItems, endOfMenuAlwaysVisible = true) }

    override val menuToolbar by lazy {
        val forward = BrowserMenuItemToolbar.TwoStateButton(
            primaryImageResource = mozilla.components.ui.icons.R.drawable.mozac_ic_forward,
            primaryContentDescription = context.getString(R.string.browser_menu_forward),
            primaryImageTintResource = ThemeManager.resolveAttribute(
                R.attr.primaryText,
                context
            ),
            isInPrimaryState = {
                context.components.core.sessionManager.selectedSession?.canGoForward ?: true
            },
            secondaryImageTintResource = ThemeManager.resolveAttribute(
                R.attr.disabled,
                context
            ),
            disableInSecondaryState = true
        ) {
            onItemTapped.invoke(ToolbarMenu.Item.Forward)
        }

        val refresh = BrowserMenuItemToolbar.TwoStateButton(
            primaryImageResource = mozilla.components.ui.icons.R.drawable.mozac_ic_refresh,
            primaryContentDescription = context.getString(R.string.browser_menu_refresh),
            primaryImageTintResource = ThemeManager.resolveAttribute(
                R.attr.primaryText,
                context
            ),
            isInPrimaryState = {
                val loading = context.components.core.sessionManager.selectedSession?.loading
                loading == false
            },
            secondaryImageResource = mozilla.components.ui.icons.R.drawable.mozac_ic_stop,
            secondaryContentDescription = context.getString(R.string.browser_menu_stop),
            secondaryImageTintResource = ThemeManager.resolveAttribute(
                R.attr.primaryText,
                context
            ),
            disableInSecondaryState = false
        ) {
            if (context.components.core.sessionManager.selectedSession?.loading == true) {
                onItemTapped.invoke(ToolbarMenu.Item.Stop)
            } else {
                onItemTapped.invoke(ToolbarMenu.Item.Reload)
            }
        }

        val share = BrowserMenuItemToolbar.Button(
            imageResource = R.drawable.mozac_ic_share,
            contentDescription = context.getString(R.string.browser_menu_share),
            iconTintColorResource = primaryTextColor(),
            listener = {
                onItemTapped.invoke(ToolbarMenu.Item.Share)
            }
        )

        registerForIsBookmarkedUpdates(sessionManager)
        val bookmark = BrowserMenuItemToolbar.TwoStateButton(
            primaryImageResource = R.drawable.ic_bookmark_filled,
            primaryContentDescription = context.getString(R.string.browser_menu_edit_bookmark),
            primaryImageTintResource = ThemeManager.resolveAttribute(
                R.attr.primaryText,
                context
            ),
            // TwoStateButton.isInPrimaryState must be synchronous, and checking bookmark state is
            // relatively slow. The best we can do here is periodically compute and cache a new "is
            // bookmarked" state, and use that whenever the menu has been opened.
            isInPrimaryState = { currentUrlIsBookmarked },
            secondaryImageResource = R.drawable.ic_bookmark_outline,
            secondaryContentDescription = context.getString(R.string.browser_menu_bookmark),
            secondaryImageTintResource = ThemeManager.resolveAttribute(
                R.attr.primaryText,
                context
            ),
            disableInSecondaryState = false
        ) {
            if (!currentUrlIsBookmarked) currentUrlIsBookmarked = true
            onItemTapped.invoke(ToolbarMenu.Item.Bookmark)
        }

        BrowserMenuItemToolbar(listOf(forward, bookmark, share, refresh))
    }

    private val menuItems by lazy {
        // Predicates that are called once, during screen init
        val shouldShowSaveToCollection = (context.asActivity() as? HomeActivity)
            ?.browsingModeManager?.mode == BrowsingMode.Normal
        val shouldDeleteDataOnQuit = Settings.getInstance(context)
            .shouldDeleteBrowsingDataOnQuit

        // Predicates that need to be repeatedly called as the session changes
        fun shouldShowAddToHomescreen(): Boolean {
            return context.components.useCases.webAppUseCases.isPinningSupported() &&
                    context.components.core.sessionManager.selectedSession != null
        }
        fun shouldShowReaderMode(): Boolean = sessionManager.selectedSession?.readerable ?: false
        fun shouldShowOpenInApp(): Boolean = sessionManager.selectedSession?.let { session ->
            val appLink =
                context.components.useCases.appLinksUseCases.appLinkRedirect
            appLink(session.url).hasExternalApp()
        } ?: false
        fun shouldShowReaderAppearance(): Boolean =
            sessionManager.selectedSession?.readerMode ?: false

        listOfNotNull(
            help,
            settings,
            library,
            desktopMode,
            addToHomescreen.apply { visible = ::shouldShowAddToHomescreen },
            findInPage,
            privateTab,
            newTab,
            reportIssue,
            if (shouldShowSaveToCollection) saveToCollection else null,
            if (shouldDeleteDataOnQuit) deleteDataOnQuit else null,
            readerMode.apply { visible = ::shouldShowReaderMode },
            readerAppearance.apply { visible = ::shouldShowReaderAppearance },
            openInApp.apply { visible = ::shouldShowOpenInApp },
            BrowserMenuDivider(),
            menuToolbar
        )
    }

    private val help = BrowserMenuImageText(
        context.getString(R.string.browser_menu_help),
        R.drawable.ic_help,
        primaryTextColor()
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.Help)
    }

    private val settings = BrowserMenuHighlightableItem(
        label = context.getString(R.string.browser_menu_settings),
        imageResource = R.drawable.ic_settings,
        iconTintColorResource = if (hasAccountProblem)
            R.color.sync_error_text_color else
            primaryTextColor(),
        textColorResource = if (hasAccountProblem)
            R.color.sync_error_text_color else
            primaryTextColor(),
        highlight = if (hasAccountProblem) {
            BrowserMenuHighlightableItem.Highlight(
                endImageResource = R.drawable.ic_alert,
                backgroundResource = R.drawable.sync_error_background_with_ripple,
                colorResource = R.color.sync_error_background_color
            )
        } else null
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.Settings)
    }

    private val library = BrowserMenuImageText(
        label = context.getString(R.string.browser_menu_your_library),
        imageResource = R.drawable.ic_library,
        iconTintColorResource = primaryTextColor()
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.Library)
    }

    private val desktopMode = BrowserMenuImageSwitch(
        imageResource = R.drawable.ic_desktop,
        label = context.getString(R.string.browser_menu_desktop_site),
        initialState = requestDesktopStateProvider
    ) { checked ->
        onItemTapped.invoke(ToolbarMenu.Item.RequestDesktop(checked))
    }

    private val addToHomescreen = BrowserMenuImageText(
        label = context.getString(R.string.browser_menu_add_to_homescreen),
        imageResource = R.drawable.ic_add_to_homescreen,
        iconTintColorResource = primaryTextColor()
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.AddToHomeScreen)
    }

    private val findInPage = BrowserMenuImageText(
        label = context.getString(R.string.browser_menu_find_in_page),
        imageResource = R.drawable.mozac_ic_search,
        iconTintColorResource = primaryTextColor()
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.FindInPage)
    }

    private val privateTab = BrowserMenuImageText(
        label = context.getString(R.string.browser_menu_private_tab),
        imageResource = R.drawable.ic_private_browsing,
        iconTintColorResource = primaryTextColor()
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.NewPrivateTab)
    }

    private val newTab = BrowserMenuImageText(
        label = context.getString(R.string.browser_menu_new_tab),
        imageResource = R.drawable.ic_new,
        iconTintColorResource = primaryTextColor()
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.NewTab)
    }

    private val reportIssue = BrowserMenuImageText(
        label = context.getString(R.string.browser_menu_report_issue),
        imageResource = R.drawable.ic_report_issues,
        iconTintColorResource = primaryTextColor()
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.ReportIssue)
    }

    private val saveToCollection = BrowserMenuImageText(
        label = context.getString(R.string.browser_menu_save_to_collection),
        imageResource = R.drawable.ic_tab_collection,
        iconTintColorResource = primaryTextColor()
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.SaveToCollection)
    }

    private val deleteDataOnQuit = BrowserMenuImageText(
        label = context.getString(R.string.delete_browsing_data_on_quit_action),
        imageResource = R.drawable.ic_exit,
        iconTintColorResource = primaryTextColor()
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.Quit)
    }

    private val readerMode = BrowserMenuImageSwitch(
        label = context.getString(R.string.browser_menu_read),
        imageResource = R.drawable.ic_readermode,
        initialState = readerModeStateProvider
    ) { checked ->
        onItemTapped.invoke(ToolbarMenu.Item.ReaderMode(checked))
    }

    private val readerAppearance = BrowserMenuImageText(
        label = context.getString(R.string.browser_menu_read_appearance),
        imageResource = R.drawable.ic_readermode_appearance,
        iconTintColorResource = primaryTextColor()
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.ReaderModeAppearance)
    }

    private val openInApp = BrowserMenuImageText(
        label = context.getString(R.string.browser_menu_open_app_link),
        imageResource = R.drawable.ic_app_links,
        iconTintColorResource = primaryTextColor()
        ) {
        onItemTapped.invoke(ToolbarMenu.Item.OpenInApp)
    }

    private fun primaryTextColor() = ThemeManager.resolveAttribute(R.attr.primaryText, context)

    private fun registerForIsBookmarkedUpdates(sessionManager: SessionManager) {
        val observer = object : Session.Observer {
            override fun onUrlChanged(session: Session, url: String) {
                currentUrlIsBookmarked = false
                updateCurrentUrlIsBookmarked(url)
            }
        }

        sessionManager.selectedSession?.url?.let { updateCurrentUrlIsBookmarked(it) }
        sessionManager.selectedSession?.register(observer, lifecycleOwner)
    }

    private fun updateCurrentUrlIsBookmarked(newUrl: String) {
        isBookmarkedJob?.cancel()
        isBookmarkedJob = lifecycleOwner.lifecycleScope.launch {
            currentUrlIsBookmarked = bookmarksStorage
                .getBookmarksWithUrl(newUrl)
                .any { it.url == newUrl }
        }
    }
}
