/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.content.Context
import androidx.core.content.ContextCompat.getColor
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.BrowserMenuHighlight
import mozilla.components.browser.menu.BrowserMenuItem
import mozilla.components.browser.menu.ext.getHighlight
import mozilla.components.browser.menu.item.BrowserMenuDivider
import mozilla.components.browser.menu.item.BrowserMenuHighlightableItem
import mozilla.components.browser.menu.item.BrowserMenuImageSwitch
import mozilla.components.browser.menu.item.BrowserMenuImageText
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.support.ktx.android.content.getColorFromAttr
import org.mozilla.fenix.R
import org.mozilla.fenix.components.accounts.AccountState
import org.mozilla.fenix.components.accounts.FenixAccountManager
import org.mozilla.fenix.experiments.FeatureId
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getVariables
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.theme.ThemeManager
import org.mozilla.fenix.whatsnew.WhatsNew

@Suppress("LargeClass", "LongMethod")
class HomeMenu(
    private val lifecycleOwner: LifecycleOwner,
    private val context: Context,
    private val onItemTapped: (Item) -> Unit = {},
    private val onMenuBuilderChanged: (BrowserMenuBuilder) -> Unit = {},
    private val onHighlightPresent: (BrowserMenuHighlight) -> Unit = {}
) {
    sealed class Item {
        object Bookmarks : Item()
        object History : Item()
        object Downloads : Item()
        object Extensions : Item()
        data class SyncAccount(val accountState: AccountState) : Item()
        object WhatsNew : Item()
        object Help : Item()
        object Settings : Item()
        object Quit : Item()
        object ReconnectSync : Item()
        data class DesktopMode(val checked: Boolean) : Item()
    }

    private val primaryTextColor = ThemeManager.resolveAttribute(R.attr.primaryText, context)
    private val syncDisconnectedColor =
        ThemeManager.resolveAttribute(R.attr.syncDisconnected, context)
    private val syncDisconnectedBackgroundColor =
        context.getColorFromAttr(R.attr.syncDisconnectedBackground)

    private val shouldUseBottomToolbar = context.settings().shouldUseBottomToolbar
    private val accountManager = FenixAccountManager(context)

    // 'Reconnect' and 'Quit' items aren't needed most of the time, so we'll only create the if necessary.
    private val reconnectToSyncItem by lazy {
        BrowserMenuHighlightableItem(
            context.getString(R.string.sync_reconnect),
            R.drawable.ic_sync_disconnected,
            iconTintColorResource = syncDisconnectedColor,
            textColorResource = primaryTextColor,
            highlight = BrowserMenuHighlight.HighPriority(
                backgroundTint = syncDisconnectedBackgroundColor,
                canPropagate = false
            ),
            isHighlighted = { true }
        ) {
            onItemTapped.invoke(Item.ReconnectSync)
        }
    }

    private val quitItem by lazy {
        BrowserMenuImageText(
            context.getString(R.string.delete_browsing_data_on_quit_action),
            R.drawable.ic_exit,
            primaryTextColor
        ) {
            onItemTapped.invoke(Item.Quit)
        }
    }

    private fun getSyncItemTitle(): String =
        accountManager.accountProfileEmail ?: context.getString(R.string.sync_menu_sign_in)

    private val syncSignInMenuItem = BrowserMenuImageText(
        getSyncItemTitle(),
        R.drawable.ic_synced_tabs,
        primaryTextColor
    ) {
        onItemTapped.invoke(Item.SyncAccount(accountManager.accountState))
    }

    val desktopItem = BrowserMenuImageSwitch(
        imageResource = R.drawable.ic_desktop,
        label = context.getString(R.string.browser_menu_desktop_site),
        initialState = { context.settings().openNextTabInDesktopMode }
    ) { checked ->
        onItemTapped.invoke(Item.DesktopMode(checked))
    }

    @Suppress("ComplexMethod")
    private fun coreMenuItems(): List<BrowserMenuItem> {
        val experiments = context.components.analytics.experiments
        val settings = context.components.settings

        val bookmarksItem = BrowserMenuImageText(
            context.getString(R.string.library_bookmarks),
            R.drawable.ic_bookmark_list,
            primaryTextColor
        ) {
            onItemTapped.invoke(Item.Bookmarks)
        }

        val historyItem = BrowserMenuImageText(
            context.getString(R.string.library_history),
            R.drawable.ic_history,
            primaryTextColor
        ) {
            onItemTapped.invoke(Item.History)
        }

        val downloadsItem = BrowserMenuImageText(
            context.getString(R.string.library_downloads),
            R.drawable.ic_download,
            primaryTextColor
        ) {
            onItemTapped.invoke(Item.Downloads)
        }

        val extensionsItem = BrowserMenuImageText(
            context.getString(R.string.browser_menu_add_ons),
            R.drawable.ic_addons_extensions,
            primaryTextColor
        ) {
            onItemTapped.invoke(Item.Extensions)
        }

        val whatsNewItem = BrowserMenuHighlightableItem(
            context.getString(R.string.browser_menu_whats_new),
            R.drawable.ic_whats_new,
            iconTintColorResource = primaryTextColor,
            highlight = BrowserMenuHighlight.LowPriority(
                notificationTint = getColor(context, R.color.whats_new_notification_color)
            ),
            isHighlighted = { WhatsNew.shouldHighlightWhatsNew(context) }
        ) {
            onItemTapped.invoke(Item.WhatsNew)
        }

        val helpItem = BrowserMenuImageText(
            context.getString(R.string.browser_menu_help),
            R.drawable.mozac_ic_help,
            primaryTextColor
        ) {
            onItemTapped.invoke(Item.Help)
        }

        // Use nimbus to set the icon and title.
        val variables = experiments.getVariables(FeatureId.NIMBUS_VALIDATION)
        val settingsItem = BrowserMenuImageText(
            variables.getText("settings-title") ?: context.getString(R.string.browser_menu_settings),
            variables.getDrawableResource("settings-icon") ?: R.drawable.mozac_ic_settings,
            primaryTextColor
        ) {
            onItemTapped.invoke(Item.Settings)
        }

        // Only query account manager if it has been initialized.
        // We don't want to cause its initialization just for this check.
        val accountAuthItem =
            if (context.components.backgroundServices.accountManagerAvailableQueue.isReady() &&
                context.components.backgroundServices.accountManager.accountNeedsReauth()
            ) {
                reconnectToSyncItem
            } else {
                null
            }

        val menuItems = listOfNotNull(
            bookmarksItem,
            historyItem,
            downloadsItem,
            extensionsItem,
            syncSignInMenuItem,
            accountAuthItem,
            BrowserMenuDivider(),
            desktopItem,
            BrowserMenuDivider(),
            whatsNewItem,
            helpItem,
            settingsItem,
            if (settings.shouldDeleteBrowsingDataOnQuit) quitItem else null
        ).also { items ->
            items.getHighlight()?.let { onHighlightPresent(it) }
        }

        return menuItems
    }

    init {
        val menuItems = coreMenuItems()

        // Report initial state.
        onMenuBuilderChanged(BrowserMenuBuilder(menuItems))

        // Observe account state changes, and update menu item builder with a new set of items.
        context.components.backgroundServices.accountManagerAvailableQueue.runIfReadyOrQueue {
            // This task isn't relevant if our parent fragment isn't around anymore.
            if (lifecycleOwner.lifecycle.currentState == Lifecycle.State.DESTROYED) {
                return@runIfReadyOrQueue
            }
            context.components.backgroundServices.accountManager.register(
                object : AccountObserver {
                    override fun onAuthenticationProblems() {
                        lifecycleOwner.lifecycleScope.launch(Dispatchers.Main) {
                            onMenuBuilderChanged(
                                BrowserMenuBuilder(
                                    menuItems
                                )
                            )
                        }
                    }

                    override fun onAuthenticated(account: OAuthAccount, authType: AuthType) {
                        lifecycleOwner.lifecycleScope.launch(Dispatchers.Main) {
                            onMenuBuilderChanged(
                                BrowserMenuBuilder(
                                    menuItems
                                )
                            )
                        }
                    }

                    override fun onLoggedOut() {
                        lifecycleOwner.lifecycleScope.launch(Dispatchers.Main) {
                            onMenuBuilderChanged(
                                BrowserMenuBuilder(
                                    menuItems
                                )
                            )
                        }
                    }
                },
                lifecycleOwner
            )
        }
    }
}
