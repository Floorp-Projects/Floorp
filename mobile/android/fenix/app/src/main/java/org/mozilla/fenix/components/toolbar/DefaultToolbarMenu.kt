/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import androidx.annotation.ColorRes
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.Companion.PRIVATE
import androidx.core.content.ContextCompat.getColor
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.mapNotNull
import kotlinx.coroutines.launch
import mozilla.components.browser.menu.BrowserMenuHighlight
import mozilla.components.browser.menu.BrowserMenuItem
import mozilla.components.browser.menu.WebExtensionBrowserMenuBuilder
import mozilla.components.browser.menu.item.BrowserMenuDivider
import mozilla.components.browser.menu.item.BrowserMenuHighlightableItem
import mozilla.components.browser.menu.item.BrowserMenuImageSwitch
import mozilla.components.browser.menu.item.BrowserMenuImageText
import mozilla.components.browser.menu.item.BrowserMenuImageTextCheckboxButton
import mozilla.components.browser.menu.item.BrowserMenuItemToolbar
import mozilla.components.browser.menu.item.TwoStateBrowserMenuImageText
import mozilla.components.browser.menu.item.WebExtensionPlaceholderMenuItem
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.storage.BookmarksStorage
import mozilla.components.feature.top.sites.PinnedSiteStorage
import mozilla.components.feature.webcompat.reporter.WebCompatReporterFeature
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.ktx.android.content.getColorFromAttr
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifAnyChanged
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.R
import org.mozilla.fenix.components.accounts.FenixAccountManager
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.nimbus.FxNimbus
import org.mozilla.fenix.theme.ThemeManager

/**
 * Builds the toolbar object used with the 3-dot menu in the browser fragment.
 * @param context a [Context] for accessing system resources.
 * @param store reference to the application's [BrowserStore].
 * @param hasAccountProblem If true, there was a problem signing into the Firefox account.
 * @param onItemTapped Called when a menu item is tapped.
 * @param lifecycleOwner View lifecycle owner used to determine when to cancel UI jobs.
 * @param bookmarksStorage Used to check if a page is bookmarked.
 * @param pinnedSiteStorage Used to check if the current url is a pinned site.
 * @param isPinningSupported true if the launcher supports adding shortcuts.
 */
@Suppress("LargeClass", "LongParameterList", "TooManyFunctions")
open class DefaultToolbarMenu(
    private val context: Context,
    private val store: BrowserStore,
    hasAccountProblem: Boolean = false,
    private val onItemTapped: (ToolbarMenu.Item) -> Unit = {},
    private val lifecycleOwner: LifecycleOwner,
    private val bookmarksStorage: BookmarksStorage,
    private val pinnedSiteStorage: PinnedSiteStorage,
    @get:VisibleForTesting internal val isPinningSupported: Boolean,
) : ToolbarMenu {

    private var isCurrentUrlPinned = false
    private var isCurrentUrlBookmarked = false
    private var isBookmarkedJob: Job? = null

    private val shouldDeleteDataOnQuit = context.settings().shouldDeleteBrowsingDataOnQuit
    private val shouldUseBottomToolbar = context.settings().shouldUseBottomToolbar
    private val shouldShowTopSites = context.settings().showTopSitesFeature
    private val accountManager = FenixAccountManager(context)

    private val selectedSession: TabSessionState?
        get() = store.state.selectedTab

    override val menuBuilder by lazy {
        FxNimbus.features.print.recordExposure()
        WebExtensionBrowserMenuBuilder(
            items = coreMenuItems,
            endOfMenuAlwaysVisible = shouldUseBottomToolbar,
            store = store,
            style = WebExtensionBrowserMenuBuilder.Style(
                webExtIconTintColorResource = primaryTextColor(),
                addonsManagerMenuItemDrawableRes = R.drawable.ic_addons_extensions,
            ),
            onAddonsManagerTapped = {
                onItemTapped.invoke(ToolbarMenu.Item.AddonsManager)
            },
            appendExtensionSubMenuAtStart = shouldUseBottomToolbar,
        )
    }

    override val menuToolbar by lazy {
        val back = BrowserMenuItemToolbar.TwoStateButton(
            primaryImageResource = mozilla.components.ui.icons.R.drawable.mozac_ic_back_24,
            primaryContentDescription = context.getString(R.string.browser_menu_back),
            primaryImageTintResource = primaryTextColor(),
            isInPrimaryState = {
                selectedSession?.content?.canGoBack ?: true
            },
            secondaryImageTintResource = ThemeManager.resolveAttribute(R.attr.textDisabled, context),
            disableInSecondaryState = true,
            longClickListener = { onItemTapped.invoke(ToolbarMenu.Item.Back(viewHistory = true)) },
        ) {
            onItemTapped.invoke(ToolbarMenu.Item.Back(viewHistory = false))
        }

        val forward = BrowserMenuItemToolbar.TwoStateButton(
            primaryImageResource = mozilla.components.ui.icons.R.drawable.mozac_ic_forward_24,
            primaryContentDescription = context.getString(R.string.browser_menu_forward),
            primaryImageTintResource = primaryTextColor(),
            isInPrimaryState = {
                selectedSession?.content?.canGoForward ?: true
            },
            secondaryImageTintResource = ThemeManager.resolveAttribute(R.attr.textDisabled, context),
            disableInSecondaryState = true,
            longClickListener = { onItemTapped.invoke(ToolbarMenu.Item.Forward(viewHistory = true)) },
        ) {
            onItemTapped.invoke(ToolbarMenu.Item.Forward(viewHistory = false))
        }

        val refresh = BrowserMenuItemToolbar.TwoStateButton(
            primaryImageResource = mozilla.components.ui.icons.R.drawable.mozac_ic_arrow_clockwise_24,
            primaryContentDescription = context.getString(R.string.browser_menu_refresh),
            primaryImageTintResource = primaryTextColor(),
            isInPrimaryState = {
                selectedSession?.content?.loading == false
            },
            secondaryImageResource = mozilla.components.ui.icons.R.drawable.mozac_ic_stop,
            secondaryContentDescription = context.getString(R.string.browser_menu_stop),
            secondaryImageTintResource = primaryTextColor(),
            disableInSecondaryState = false,
            longClickListener = { onItemTapped.invoke(ToolbarMenu.Item.Reload(bypassCache = true)) },
        ) {
            if (selectedSession?.content?.loading == true) {
                onItemTapped.invoke(ToolbarMenu.Item.Stop)
            } else {
                onItemTapped.invoke(ToolbarMenu.Item.Reload(bypassCache = false))
            }
        }

        val share = BrowserMenuItemToolbar.Button(
            imageResource = R.drawable.ic_share,
            contentDescription = context.getString(R.string.browser_menu_share),
            iconTintColorResource = primaryTextColor(),
            listener = {
                onItemTapped.invoke(ToolbarMenu.Item.Share)
            },
        )

        registerForIsBookmarkedUpdates()

        BrowserMenuItemToolbar(listOf(back, forward, share, refresh), isSticky = true)
    }

    // Predicates that need to be repeatedly called as the session changes
    @VisibleForTesting(otherwise = PRIVATE)
    fun canAddToHomescreen(): Boolean =
        selectedSession != null && isPinningSupported &&
            !context.components.useCases.webAppUseCases.isInstallable()

    @VisibleForTesting(otherwise = PRIVATE)
    fun canInstall(): Boolean =
        selectedSession != null && isPinningSupported &&
            context.components.useCases.webAppUseCases.isInstallable()

    @VisibleForTesting(otherwise = PRIVATE)
    fun shouldShowOpenInApp(): Boolean = selectedSession?.let { session ->
        val appLink = context.components.useCases.appLinksUseCases.appLinkRedirect
        appLink(session.content.url).hasExternalApp()
    } ?: false

    @VisibleForTesting(otherwise = PRIVATE)
    fun shouldShowReaderViewCustomization(): Boolean = selectedSession?.let {
        store.state.findTab(it.id)?.readerState?.active
    } ?: false
    // End of predicates //

    private val installToHomescreen = BrowserMenuHighlightableItem(
        label = context.getString(R.string.browser_menu_install_on_homescreen),
        startImageResource = R.drawable.mozac_ic_add_to_home_screen_24,
        iconTintColorResource = primaryTextColor(),
        highlight = BrowserMenuHighlight.LowPriority(
            label = context.getString(R.string.browser_menu_install_on_homescreen),
            notificationTint = getColor(context, R.color.fx_mobile_icon_color_information),
        ),
        isCollapsingMenuLimit = true,
        isHighlighted = {
            !context.settings().installPwaOpened
        },
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.InstallPwaToHomeScreen)
    }

    @VisibleForTesting
    internal val newTabItem = BrowserMenuImageText(
        context.getString(R.string.library_new_tab),
        R.drawable.ic_new,
        primaryTextColor(),
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.NewTab)
    }

    private val historyItem = BrowserMenuImageText(
        context.getString(R.string.library_history),
        R.drawable.ic_history,
        primaryTextColor(),
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.History)
    }

    private val downloadsItem = BrowserMenuImageText(
        context.getString(R.string.library_downloads),
        R.drawable.ic_download,
        primaryTextColor(),
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.Downloads)
    }

    private val extensionsItem = WebExtensionPlaceholderMenuItem(
        id = WebExtensionPlaceholderMenuItem.MAIN_EXTENSIONS_MENU_ID,
    )

    private val findInPageItem = BrowserMenuImageText(
        label = context.getString(R.string.browser_menu_find_in_page),
        imageResource = R.drawable.mozac_ic_search_24,
        iconTintColorResource = primaryTextColor(),
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.FindInPage)
    }

    private val desktopSiteItem = BrowserMenuImageSwitch(
        imageResource = R.drawable.ic_desktop,
        label = context.getString(R.string.browser_menu_desktop_site),
        initialState = {
            selectedSession?.content?.desktopMode ?: false
        },
    ) { checked ->
        onItemTapped.invoke(ToolbarMenu.Item.RequestDesktop(checked))
    }

    private val customizeReaderView = BrowserMenuImageText(
        label = context.getString(R.string.browser_menu_customize_reader_view),
        imageResource = R.drawable.ic_readermode_appearance,
        iconTintColorResource = primaryTextColor(),
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.CustomizeReaderView)
    }

    private val openInApp = BrowserMenuHighlightableItem(
        label = context.getString(R.string.browser_menu_open_app_link),
        startImageResource = R.drawable.ic_open_in_app,
        iconTintColorResource = primaryTextColor(),
        highlight = BrowserMenuHighlight.LowPriority(
            label = context.getString(R.string.browser_menu_open_app_link),
            notificationTint = getColor(context, R.color.fx_mobile_icon_color_information),
        ),
        isHighlighted = { !context.settings().openInAppOpened },
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.OpenInApp)
    }

    private val reportSiteIssuePlaceholder = WebExtensionPlaceholderMenuItem(
        id = WebCompatReporterFeature.WEBCOMPAT_REPORTER_EXTENSION_ID,
        iconTintColorResource = primaryTextColor(),
    )

    private val addToHomeScreenItem = BrowserMenuImageText(
        label = context.getString(R.string.browser_menu_add_to_homescreen),
        imageResource = R.drawable.mozac_ic_add_to_home_screen_24,
        iconTintColorResource = primaryTextColor(),
        isCollapsingMenuLimit = true,
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.AddToHomeScreen)
    }

    private val addRemoveTopSitesItem = TwoStateBrowserMenuImageText(
        primaryLabel = context.getString(R.string.browser_menu_add_to_shortcuts),
        secondaryLabel = context.getString(R.string.browser_menu_remove_from_shortcuts),
        primaryStateIconResource = R.drawable.ic_top_sites,
        secondaryStateIconResource = R.drawable.ic_top_sites,
        iconTintColorResource = primaryTextColor(),
        isInPrimaryState = { !isCurrentUrlPinned },
        isInSecondaryState = { isCurrentUrlPinned },
        primaryStateAction = {
            isCurrentUrlPinned = true
            onItemTapped.invoke(ToolbarMenu.Item.AddToTopSites)
        },
        secondaryStateAction = {
            isCurrentUrlPinned = false
            onItemTapped.invoke(ToolbarMenu.Item.RemoveFromTopSites)
        },
    )

    private val saveToCollectionItem = BrowserMenuImageText(
        label = context.getString(R.string.browser_menu_save_to_collection_2),
        imageResource = R.drawable.ic_tab_collection,
        iconTintColorResource = primaryTextColor(),
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.SaveToCollection)
    }

    private val printPageItem = BrowserMenuImageText(
        label = context.getString(R.string.menu_print),
        imageResource = R.drawable.ic_print,
        iconTintColorResource = primaryTextColor(),
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.PrintContent)
    }

    @VisibleForTesting
    internal val settingsItem = BrowserMenuHighlightableItem(
        label = context.getString(R.string.browser_menu_settings),
        startImageResource = R.drawable.mozac_ic_settings_24,
        iconTintColorResource = if (hasAccountProblem) {
            ThemeManager.resolveAttribute(R.attr.syncDisconnected, context)
        } else {
            primaryTextColor()
        },
        textColorResource = if (hasAccountProblem) {
            ThemeManager.resolveAttribute(R.attr.textPrimary, context)
        } else {
            primaryTextColor()
        },
        highlight = BrowserMenuHighlight.HighPriority(
            endImageResource = R.drawable.ic_sync_disconnected,
            backgroundTint = context.getColorFromAttr(R.attr.syncDisconnectedBackground),
            canPropagate = false,
        ),
        isHighlighted = { hasAccountProblem },
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.Settings)
    }

    private val bookmarksItem = BrowserMenuImageTextCheckboxButton(
        imageResource = R.drawable.ic_bookmarks_menu,
        iconTintColorResource = primaryTextColor(),
        label = context.getString(R.string.library_bookmarks),
        labelListener = {
            onItemTapped.invoke(ToolbarMenu.Item.Bookmarks)
        },
        primaryStateIconResource = R.drawable.ic_bookmark_outline,
        secondaryStateIconResource = R.drawable.ic_bookmark_filled,
        tintColorResource = menuItemButtonTintColor(),
        primaryLabel = context.getString(R.string.browser_menu_add),
        secondaryLabel = context.getString(R.string.browser_menu_edit),
        isInPrimaryState = { !isCurrentUrlBookmarked },
    ) {
        handleBookmarkItemTapped()
    }

    private val deleteDataOnQuit = BrowserMenuImageText(
        label = context.getString(R.string.delete_browsing_data_on_quit_action),
        imageResource = R.drawable.mozac_ic_cross_circle_24,
        iconTintColorResource = primaryTextColor(),
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.Quit)
    }

    private fun syncMenuItem(): BrowserMenuItem {
        return BrowserMenuSignIn(primaryTextColor()) {
            onItemTapped.invoke(
                ToolbarMenu.Item.SyncAccount(accountManager.accountState),
            )
        }
    }

    @VisibleForTesting(otherwise = PRIVATE)
    val coreMenuItems by lazy {
        val menuItems =
            listOfNotNull(
                if (shouldUseBottomToolbar) null else menuToolbar,
                newTabItem,
                BrowserMenuDivider(),
                bookmarksItem,
                historyItem,
                downloadsItem,
                extensionsItem,
                syncMenuItem(),
                BrowserMenuDivider(),
                findInPageItem,
                desktopSiteItem,
                customizeReaderView.apply { visible = ::shouldShowReaderViewCustomization },
                openInApp.apply { visible = ::shouldShowOpenInApp },
                reportSiteIssuePlaceholder,
                BrowserMenuDivider(),
                addToHomeScreenItem.apply { visible = ::canAddToHomescreen },
                installToHomescreen.apply { visible = ::canInstall },
                if (shouldShowTopSites) addRemoveTopSitesItem else null,
                saveToCollectionItem,
                if (FeatureFlags.print && FxNimbus.features.print.value().browserPrintEnabled) printPageItem else null,
                BrowserMenuDivider(),
                settingsItem,
                if (shouldDeleteDataOnQuit) deleteDataOnQuit else null,
                if (shouldUseBottomToolbar) BrowserMenuDivider() else null,
                if (shouldUseBottomToolbar) menuToolbar else null,
            )

        menuItems
    }

    private fun handleBookmarkItemTapped() {
        if (!isCurrentUrlBookmarked) isCurrentUrlBookmarked = true
        onItemTapped.invoke(ToolbarMenu.Item.Bookmark)
    }

    @ColorRes
    @VisibleForTesting
    internal fun primaryTextColor() = ThemeManager.resolveAttribute(R.attr.textPrimary, context)

    @ColorRes
    @VisibleForTesting
    internal fun menuItemButtonTintColor() = ThemeManager.resolveAttribute(R.attr.menuItemButtonTintColor, context)

    @VisibleForTesting
    internal fun updateIsCurrentUrlPinned(currentUrl: String) {
        lifecycleOwner.lifecycleScope.launch {
            isCurrentUrlPinned = pinnedSiteStorage
                .getPinnedSites()
                .find { it.url == currentUrl } != null
        }
    }

    @VisibleForTesting
    internal fun registerForIsBookmarkedUpdates() {
        store.flowScoped(lifecycleOwner) { flow ->
            flow.mapNotNull { state -> state.selectedTab }
                .ifAnyChanged { tab ->
                    arrayOf(
                        tab.id,
                        tab.content.url,
                    )
                }
                .collect {
                    isCurrentUrlPinned = false
                    updateIsCurrentUrlPinned(it.content.url)

                    isCurrentUrlBookmarked = false
                    updateCurrentUrlIsBookmarked(it.content.url)
                }
        }
    }

    @VisibleForTesting
    internal fun updateCurrentUrlIsBookmarked(newUrl: String) {
        isBookmarkedJob?.cancel()
        isBookmarkedJob = lifecycleOwner.lifecycleScope.launch {
            isCurrentUrlBookmarked = bookmarksStorage
                .getBookmarksWithUrl(newUrl)
                .any { it.url == newUrl }
        }
    }
}
