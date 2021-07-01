/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import androidx.annotation.ColorRes
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PRIVATE
import androidx.core.content.ContextCompat.getColor
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.mapNotNull
import kotlinx.coroutines.launch
import mozilla.components.browser.menu.BrowserMenuHighlight
import mozilla.components.browser.menu.WebExtensionBrowserMenuBuilder
import mozilla.components.browser.menu.item.BrowserMenuDivider
import mozilla.components.browser.menu.item.BrowserMenuHighlightableItem
import mozilla.components.browser.menu.item.BrowserMenuImageSwitch
import mozilla.components.browser.menu.item.BrowserMenuImageText
import mozilla.components.browser.menu.item.BrowserMenuImageTextCheckboxButton
import mozilla.components.browser.menu.item.BrowserMenuItemToolbar
import mozilla.components.browser.menu.item.WebExtensionPlaceholderMenuItem
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.storage.BookmarksStorage
import mozilla.components.feature.webcompat.reporter.WebCompatReporterFeature
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.ktx.android.content.getColorFromAttr
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifAnyChanged
import org.mozilla.fenix.R
import org.mozilla.fenix.components.accounts.FenixAccountManager
import org.mozilla.fenix.experiments.ExperimentBranch
import org.mozilla.fenix.experiments.FeatureId
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.withExperiment
import org.mozilla.fenix.theme.ThemeManager
import org.mozilla.fenix.utils.BrowsersCache

/**
 * Builds the toolbar object used with the 3-dot menu in the browser fragment.
 * @param store reference to the application's [BrowserStore].
 * @param hasAccountProblem If true, there was a problem signing into the Firefox account.
 * @param shouldReverseItems If true, reverse the menu items.
 * @param onItemTapped Called when a menu item is tapped.
 * @param lifecycleOwner View lifecycle owner used to determine when to cancel UI jobs.
 * @param bookmarksStorage Used to check if a page is bookmarked.
 */
@Suppress("LargeClass", "LongParameterList", "TooManyFunctions")
@ExperimentalCoroutinesApi
open class DefaultToolbarMenu(
    private val context: Context,
    private val store: BrowserStore,
    hasAccountProblem: Boolean = false,
    private val onItemTapped: (ToolbarMenu.Item) -> Unit = {},
    private val lifecycleOwner: LifecycleOwner,
    private val bookmarksStorage: BookmarksStorage,
    val isPinningSupported: Boolean
) : ToolbarMenu {

    private var isCurrentUrlBookmarked = false
    private var isBookmarkedJob: Job? = null

    private val shouldDeleteDataOnQuit = context.settings().shouldDeleteBrowsingDataOnQuit
    private val shouldUseBottomToolbar = context.settings().shouldUseBottomToolbar
    private val accountManager = FenixAccountManager(context)

    private val selectedSession: TabSessionState?
        get() = store.state.selectedTab

    override val menuBuilder by lazy {
        WebExtensionBrowserMenuBuilder(
            items = coreMenuItems,
            endOfMenuAlwaysVisible = shouldUseBottomToolbar,
            store = store,
            style = WebExtensionBrowserMenuBuilder.Style(
                webExtIconTintColorResource = primaryTextColor(),
                addonsManagerMenuItemDrawableRes = R.drawable.ic_addons_extensions
            ),
            onAddonsManagerTapped = {
                onItemTapped.invoke(ToolbarMenu.Item.AddonsManager)
            },
            appendExtensionSubMenuAtStart = shouldUseBottomToolbar
        )
    }

    override val menuToolbar by lazy {
        val back = BrowserMenuItemToolbar.TwoStateButton(
            primaryImageResource = mozilla.components.ui.icons.R.drawable.mozac_ic_back,
            primaryContentDescription = context.getString(R.string.browser_menu_back),
            primaryImageTintResource = primaryTextColor(),
            isInPrimaryState = {
                selectedSession?.content?.canGoBack ?: true
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
                selectedSession?.content?.canGoForward ?: true
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
                selectedSession?.content?.loading == false
            },
            secondaryImageResource = mozilla.components.ui.icons.R.drawable.mozac_ic_stop,
            secondaryContentDescription = context.getString(R.string.browser_menu_stop),
            secondaryImageTintResource = primaryTextColor(),
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
            imageResource = R.drawable.ic_share,
            contentDescription = context.getString(R.string.browser_menu_share),
            iconTintColorResource = primaryTextColor(),
            listener = {
                onItemTapped.invoke(ToolbarMenu.Item.Share)
            }
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

    val installToHomescreen = BrowserMenuHighlightableItem(
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
        onItemTapped.invoke(ToolbarMenu.Item.InstallPwaToHomeScreen)
    }

    val newTabItem = BrowserMenuImageText(
        context.getString(R.string.library_new_tab),
        R.drawable.ic_new,
        primaryTextColor()
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.NewTab)
    }

    val historyItem = BrowserMenuImageText(
        context.getString(R.string.library_history),
        R.drawable.ic_history,
        primaryTextColor()
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.History)
    }

    val downloadsItem = BrowserMenuImageText(
        context.getString(R.string.library_downloads),
        R.drawable.ic_download,
        primaryTextColor()
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.Downloads)
    }

    val extensionsItem = WebExtensionPlaceholderMenuItem(
        id = WebExtensionPlaceholderMenuItem.MAIN_EXTENSIONS_MENU_ID
    )

    val findInPageItem = BrowserMenuImageText(
        label = context.getString(R.string.browser_menu_find_in_page),
        imageResource = R.drawable.mozac_ic_search,
        iconTintColorResource = primaryTextColor()
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.FindInPage)
    }

    val desktopSiteItem = BrowserMenuImageSwitch(
        imageResource = R.drawable.ic_desktop,
        label = context.getString(R.string.browser_menu_desktop_site),
        initialState = {
            selectedSession?.content?.desktopMode ?: false
        }
    ) { checked ->
        onItemTapped.invoke(ToolbarMenu.Item.RequestDesktop(checked))
    }

    val customizeReaderView = BrowserMenuImageText(
        label = context.getString(R.string.browser_menu_customize_reader_view),
        imageResource = R.drawable.ic_readermode_appearance,
        iconTintColorResource = primaryTextColor()
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.CustomizeReaderView)
    }

    val openInApp = BrowserMenuHighlightableItem(
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

    val reportSiteIssuePlaceholder = WebExtensionPlaceholderMenuItem(
        id = WebCompatReporterFeature.WEBCOMPAT_REPORTER_EXTENSION_ID,
        iconTintColorResource = primaryTextColor()
    )

    val addToHomeScreenItem = BrowserMenuImageText(
        label = context.getString(R.string.browser_menu_add_to_homescreen),
        imageResource = R.drawable.ic_add_to_homescreen,
        iconTintColorResource = primaryTextColor(),
        isCollapsingMenuLimit = true
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.AddToHomeScreen)
    }

    val addToTopSitesItem = BrowserMenuImageText(
        label = context.getString(R.string.browser_menu_add_to_top_sites),
        imageResource = R.drawable.ic_top_sites,
        iconTintColorResource = primaryTextColor()
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.AddToTopSites)
    }

    val saveToCollectionItem = BrowserMenuImageText(
        label = context.getString(R.string.browser_menu_save_to_collection_2),
        imageResource = R.drawable.ic_tab_collection,
        iconTintColorResource = primaryTextColor()
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.SaveToCollection)
    }

    val settingsItem = BrowserMenuHighlightableItem(
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

    val bookmarksItem = BrowserMenuImageTextCheckboxButton(
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
        isInPrimaryState = { !isCurrentUrlBookmarked }
    ) {
        handleBookmarkItemTapped()
    }

    val deleteDataOnQuit = BrowserMenuImageText(
        label = context.getString(R.string.delete_browsing_data_on_quit_action),
        imageResource = R.drawable.ic_exit,
        iconTintColorResource = primaryTextColor()
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.Quit)
    }

    private fun getSyncItemTitle() =
        accountManager.accountProfileEmail ?: context.getString(R.string.sync_menu_sign_in)

    val syncMenuItem = BrowserMenuImageText(
        getSyncItemTitle(),
        R.drawable.ic_signed_out,
        primaryTextColor()
    ) {
        onItemTapped.invoke(
            ToolbarMenu.Item.SyncAccount(accountManager.accountState)
        )
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
                syncMenuItem,
                BrowserMenuDivider(),
                getSetDefaultBrowserItem(),
                getSetDefaultBrowserItem()?.let { BrowserMenuDivider() },
                findInPageItem,
                desktopSiteItem,
                customizeReaderView.apply { visible = ::shouldShowReaderViewCustomization },
                openInApp.apply { visible = ::shouldShowOpenInApp },
                reportSiteIssuePlaceholder,
                BrowserMenuDivider(),
                addToHomeScreenItem.apply { visible = ::canAddToHomescreen },
                installToHomescreen.apply { visible = ::canInstall },
                addToTopSitesItem,
                saveToCollectionItem,
                BrowserMenuDivider(),
                settingsItem,
                if (shouldDeleteDataOnQuit) deleteDataOnQuit else null,
                if (shouldUseBottomToolbar) BrowserMenuDivider() else null,
                if (shouldUseBottomToolbar) menuToolbar else null
            )

        menuItems
    }

    private fun handleBookmarkItemTapped() {
        if (!isCurrentUrlBookmarked) isCurrentUrlBookmarked = true
        onItemTapped.invoke(ToolbarMenu.Item.Bookmark)
    }

    @ColorRes
    @VisibleForTesting
    internal fun primaryTextColor() = ThemeManager.resolveAttribute(R.attr.primaryText, context)

    @ColorRes
    @VisibleForTesting
    internal fun menuItemButtonTintColor() = ThemeManager.resolveAttribute(R.attr.menuItemButtonTintColor, context)

    @VisibleForTesting
    internal fun registerForIsBookmarkedUpdates() {
        store.flowScoped(lifecycleOwner) { flow ->
            flow.mapNotNull { state -> state.selectedTab }
                .ifAnyChanged { tab ->
                    arrayOf(
                        tab.id,
                        tab.content.url
                    )
                }
                .collect {
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

    private fun getSetDefaultBrowserItem(): BrowserMenuImageText? {
        val experiments = context.components.analytics.experiments
        val browsers = BrowsersCache.all(context)

        return experiments.withExperiment(FeatureId.DEFAULT_BROWSER) { experimentBranch ->
            if (experimentBranch == ExperimentBranch.DEFAULT_BROWSER_TOOLBAR_MENU &&
                !browsers.isFirefoxDefaultBrowser
            ) {
                return@withExperiment BrowserMenuImageText(
                    label = context.getString(R.string.preferences_set_as_default_browser),
                    imageResource = R.mipmap.ic_launcher
                ) {
                    onItemTapped.invoke(ToolbarMenu.Item.SetDefaultBrowser)
                }
            } else {
                null
            }
        }
    }
}
