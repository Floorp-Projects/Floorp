/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import androidx.annotation.ColorRes
import androidx.core.content.ContextCompat.getColor
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.components.browser.menu.BrowserMenuHighlight
import mozilla.components.browser.menu.WebExtensionBrowserMenuBuilder
import mozilla.components.browser.menu.item.BrowserMenuDivider
import mozilla.components.browser.menu.item.BrowserMenuHighlightableItem
import mozilla.components.browser.menu.item.BrowserMenuImageSwitch
import mozilla.components.browser.menu.item.BrowserMenuImageText
import mozilla.components.browser.menu.item.BrowserMenuItemToolbar
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.storage.BookmarksStorage
import mozilla.components.support.ktx.android.content.getColorFromAttr
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.ext.asActivity
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.theme.ThemeManager

/**
 * Builds the toolbar object used with the 3-dot menu in the browser fragment.
 * @param sessionManager Reference to the session manager that contains all tabs.
 * @param hasAccountProblem If true, there was a problem signing into the Firefox account.
 * @param shouldReverseItems If true, reverse the menu items.
 * @param onItemTapped Called when a menu item is tapped.
 * @param lifecycleOwner View lifecycle owner used to determine when to cancel UI jobs.
 * @param bookmarksStorage Used to check if a page is bookmarked.
 */
@Suppress("LargeClass", "LongParameterList")
class DefaultToolbarMenu(
    private val context: Context,
    private val sessionManager: SessionManager,
    private val store: BrowserStore,
    hasAccountProblem: Boolean = false,
    shouldReverseItems: Boolean,
    private val onItemTapped: (ToolbarMenu.Item) -> Unit = {},
    private val lifecycleOwner: LifecycleOwner,
    private val bookmarksStorage: BookmarksStorage
) : ToolbarMenu {

    private var currentUrlIsBookmarked = false
    private var isBookmarkedJob: Job? = null

    /** Gets the current browser session */
    private val session: Session? get() = sessionManager.selectedSession

    override val menuBuilder by lazy {
        WebExtensionBrowserMenuBuilder(
            menuItems,
            endOfMenuAlwaysVisible = !shouldReverseItems,
            store = store,
            webExtIconTintColorResource = primaryTextColor(),
            onAddonsManagerTapped = {
                onItemTapped.invoke(ToolbarMenu.Item.AddonsManager)
            },
            appendExtensionSubMenuAtStart = !shouldReverseItems
        )
    }

    override val menuToolbar by lazy {
        val back = BrowserMenuItemToolbar.TwoStateButton(
            primaryImageResource = mozilla.components.ui.icons.R.drawable.mozac_ic_back,
            primaryContentDescription = context.getString(R.string.browser_menu_back),
            primaryImageTintResource = primaryTextColor(),
            isInPrimaryState = {
                session?.canGoBack ?: true
            },
            secondaryImageTintResource = ThemeManager.resolveAttribute(R.attr.disabled, context),
            disableInSecondaryState = true,
            longClickListener = { onItemTapped.invoke(ToolbarMenu.Item.Back(viewHistory = true)) }
        ) {
            onItemTapped.invoke(ToolbarMenu.Item.Back(viewHistory = false))
        }

        val forward = BrowserMenuItemToolbar.TwoStateButton(
            primaryImageResource = mozilla.components.ui.icons.R.drawable.mozac_ic_forward,
            primaryContentDescription = context.getString(R.string.browser_menu_forward),
            primaryImageTintResource = primaryTextColor(),
            isInPrimaryState = {
                session?.canGoForward ?: true
            },
            secondaryImageTintResource = ThemeManager.resolveAttribute(R.attr.disabled, context),
            disableInSecondaryState = true,
            longClickListener = { onItemTapped.invoke(ToolbarMenu.Item.Forward(viewHistory = true)) }
        ) {
            onItemTapped.invoke(ToolbarMenu.Item.Forward(viewHistory = false))
        }

        val refresh = BrowserMenuItemToolbar.TwoStateButton(
            primaryImageResource = mozilla.components.ui.icons.R.drawable.mozac_ic_refresh,
            primaryContentDescription = context.getString(R.string.browser_menu_refresh),
            primaryImageTintResource = primaryTextColor(),
            isInPrimaryState = {
                session?.loading == false
            },
            secondaryImageResource = mozilla.components.ui.icons.R.drawable.mozac_ic_stop,
            secondaryContentDescription = context.getString(R.string.browser_menu_stop),
            secondaryImageTintResource = primaryTextColor(),
            disableInSecondaryState = false,
            longClickListener = { onItemTapped.invoke(ToolbarMenu.Item.Reload(bypassCache = true)) }
        ) {
            if (session?.loading == true) {
                onItemTapped.invoke(ToolbarMenu.Item.Stop)
            } else {
                onItemTapped.invoke(ToolbarMenu.Item.Reload(bypassCache = false))
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

        registerForIsBookmarkedUpdates()
        val bookmark = BrowserMenuItemToolbar.TwoStateButton(
            primaryImageResource = R.drawable.ic_bookmark_filled,
            primaryContentDescription = context.getString(R.string.browser_menu_edit_bookmark),
            primaryImageTintResource = primaryTextColor(),
            // TwoStateButton.isInPrimaryState must be synchronous, and checking bookmark state is
            // relatively slow. The best we can do here is periodically compute and cache a new "is
            // bookmarked" state, and use that whenever the menu has been opened.
            isInPrimaryState = { currentUrlIsBookmarked },
            secondaryImageResource = R.drawable.ic_bookmark_outline,
            secondaryContentDescription = context.getString(R.string.browser_menu_bookmark),
            secondaryImageTintResource = primaryTextColor(),
            disableInSecondaryState = false
        ) {
            if (!currentUrlIsBookmarked) currentUrlIsBookmarked = true
            onItemTapped.invoke(ToolbarMenu.Item.Bookmark)
        }

        BrowserMenuItemToolbar(listOf(back, forward, bookmark, share, refresh))
    }

    // Predicates that need to be repeatedly called as the session changes
    private fun canAddToHomescreen(): Boolean =
        session != null && context.components.useCases.webAppUseCases.isPinningSupported() &&
                !context.components.useCases.webAppUseCases.isInstallable()

    private fun canInstall(): Boolean =
        session != null && context.components.useCases.webAppUseCases.isPinningSupported() &&
                context.components.useCases.webAppUseCases.isInstallable()

    private fun shouldShowOpenInApp(): Boolean = session?.let { session ->
        val appLink = context.components.useCases.appLinksUseCases.appLinkRedirect
        appLink(session.url).hasExternalApp()
    } ?: false

    private fun shouldShowReaderAppearance(): Boolean = session?.let {
        store.state.findTab(it.id)?.readerState?.active
    } ?: false
    // End of predicates //

    private val menuItems by lazy {
        // Predicates that are called once, during screen init
        val shouldShowSaveToCollection = (context.asActivity() as? HomeActivity)
            ?.browsingModeManager?.mode == BrowsingMode.Normal
        val shouldDeleteDataOnQuit = context.components.settings
            .shouldDeleteBrowsingDataOnQuit

        val menuItems = listOfNotNull(
            if (FeatureFlags.viewDownloads) downloadsItem else null,
            historyItem,
            bookmarksItem,
            if (FeatureFlags.syncedTabs) syncedTabs else null,
            settings,
            if (shouldDeleteDataOnQuit) deleteDataOnQuit else null,
            BrowserMenuDivider(),
            findInPage,
            addToTopSites,
            addToHomescreen.apply { visible = ::canAddToHomescreen },
            installToHomescreen.apply { visible = ::canInstall },
            if (shouldShowSaveToCollection) saveToCollection else null,
            desktopMode,
            openInApp.apply { visible = ::shouldShowOpenInApp },
            readerAppearance.apply { visible = ::shouldShowReaderAppearance },
            BrowserMenuDivider(),
            menuToolbar
        )

        if (shouldReverseItems) {
            menuItems.reversed()
        } else {
            menuItems
        }
    }

    private val settings = BrowserMenuHighlightableItem(
        label = context.getString(R.string.browser_menu_settings),
        startImageResource = R.drawable.ic_settings,
        iconTintColorResource = if (hasAccountProblem)
            ThemeManager.resolveAttribute(R.attr.syncDisconnected, context) else
            primaryTextColor(),
        textColorResource = if (hasAccountProblem)
            ThemeManager.resolveAttribute(R.attr.primaryText, context) else
            primaryTextColor(),
        highlight = BrowserMenuHighlight.HighPriority(
            endImageResource = R.drawable.ic_sync_disconnected,
            backgroundTint = context.getColorFromAttr(R.attr.syncDisconnectedBackground),
            canPropagate = false
        ),
        isHighlighted = { hasAccountProblem }
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.Settings)
    }

    private val desktopMode = BrowserMenuImageSwitch(
        imageResource = R.drawable.ic_desktop,
        label = context.getString(R.string.browser_menu_desktop_site),
        initialState = {
            session?.desktopMode ?: false
        }
    ) { checked ->
        onItemTapped.invoke(ToolbarMenu.Item.RequestDesktop(checked))
    }

    private val addToTopSites = BrowserMenuImageText(
        label = context.getString(R.string.browser_menu_add_to_top_sites),
        imageResource = R.drawable.ic_top_sites,
        iconTintColorResource = primaryTextColor()
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.AddToTopSites)
    }

    private val addToHomescreen = BrowserMenuImageText(
        label = context.getString(R.string.browser_menu_add_to_homescreen),
        imageResource = R.drawable.ic_add_to_homescreen,
        iconTintColorResource = primaryTextColor()
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.AddToHomeScreen)
    }

    private val syncedTabs = BrowserMenuImageText(
        label = context.getString(R.string.synced_tabs),
        imageResource = R.drawable.ic_synced_tabs,
        iconTintColorResource = primaryTextColor()
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.SyncedTabs)
    }

    private val installToHomescreen = BrowserMenuHighlightableItem(
        label = context.getString(R.string.browser_menu_install_on_homescreen),
        startImageResource = R.drawable.ic_add_to_homescreen,
        iconTintColorResource = primaryTextColor(),
        highlight = BrowserMenuHighlight.LowPriority(
            label = context.getString(R.string.browser_menu_install_on_homescreen),
            notificationTint = getColor(context, R.color.whats_new_notification_color)
        ),
        isHighlighted = {
            !context.settings().installPwaOpened
        }
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.InstallToHomeScreen)
    }

    private val findInPage = BrowserMenuImageText(
        label = context.getString(R.string.browser_menu_find_in_page),
        imageResource = R.drawable.mozac_ic_search,
        iconTintColorResource = primaryTextColor()
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.FindInPage)
    }

    private val saveToCollection = BrowserMenuImageText(
        label = context.getString(R.string.browser_menu_save_to_collection_2),
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

    private val readerAppearance = BrowserMenuImageText(
        label = context.getString(R.string.browser_menu_read_appearance),
        imageResource = R.drawable.ic_readermode_appearance,
        iconTintColorResource = primaryTextColor()
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.ReaderModeAppearance)
    }

    private val openInApp = BrowserMenuHighlightableItem(
        label = context.getString(R.string.browser_menu_open_app_link),
        startImageResource = R.drawable.ic_open_in_app,
        iconTintColorResource = primaryTextColor(),
        highlight = BrowserMenuHighlight.LowPriority(
            label = context.getString(R.string.browser_menu_open_app_link),
            notificationTint = getColor(context, R.color.whats_new_notification_color)
        ),
        isHighlighted = { !context.settings().openInAppOpened }
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.OpenInApp)
    }

    val historyItem = BrowserMenuImageText(
        context.getString(R.string.library_history),
        R.drawable.ic_history,
        primaryTextColor()
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.History)
    }

    val bookmarksItem = BrowserMenuImageText(
        context.getString(R.string.library_bookmarks),
        R.drawable.ic_bookmark_filled,
        primaryTextColor()
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.Bookmarks)
    }

    val downloadsItem = BrowserMenuImageText(
        "Downloads",
        R.drawable.ic_download,
        primaryTextColor()
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.Downloads)
    }

    @ColorRes
    private fun primaryTextColor() = ThemeManager.resolveAttribute(R.attr.primaryText, context)

    private var currentSessionObserver: Pair<Session, Session.Observer>? = null

    private fun registerForIsBookmarkedUpdates() {
        session?.let {
            registerForUrlChanges(it)
        }

        val sessionManagerObserver = object : SessionManager.Observer {
            override fun onSessionSelected(session: Session) {
                // Unregister any old session observer before registering a new session observer
                currentSessionObserver?.let {
                    it.first.unregister(it.second)
                }
                currentUrlIsBookmarked = false
                updateCurrentUrlIsBookmarked(session.url)
                registerForUrlChanges(session)
            }
        }

        sessionManager.register(sessionManagerObserver, lifecycleOwner)
    }

    private fun registerForUrlChanges(session: Session) {
        val sessionObserver = object : Session.Observer {
            override fun onUrlChanged(session: Session, url: String) {
                currentUrlIsBookmarked = false
                updateCurrentUrlIsBookmarked(url)
            }
        }

        currentSessionObserver = Pair(session, sessionObserver)
        updateCurrentUrlIsBookmarked(session.url)
        session.register(sessionObserver, lifecycleOwner)
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
