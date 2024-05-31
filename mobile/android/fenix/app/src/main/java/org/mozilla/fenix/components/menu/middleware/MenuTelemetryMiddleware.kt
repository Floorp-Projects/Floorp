/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.middleware

import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.GleanMetrics.AppMenu
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.GleanMetrics.HomeMenu
import org.mozilla.fenix.GleanMetrics.HomeScreen
import org.mozilla.fenix.GleanMetrics.Translations
import org.mozilla.fenix.components.menu.MenuAccessPoint
import org.mozilla.fenix.components.menu.store.MenuAction
import org.mozilla.fenix.components.menu.store.MenuState
import org.mozilla.fenix.components.menu.store.MenuStore

/**
 * A [Middleware] for recording telemetry based on [MenuAction]s that are dispatch to the
 * [MenuStore].
 *
 * @param accessPoint The [MenuAccessPoint] that was used to navigate to the menu dialog.
 */
class MenuTelemetryMiddleware(
    private val accessPoint: MenuAccessPoint,
) : Middleware<MenuState, MenuAction> {

    @Suppress("CyclomaticComplexMethod", "LongMethod")
    override fun invoke(
        context: MiddlewareContext<MenuState, MenuAction>,
        next: (MenuAction) -> Unit,
        action: MenuAction,
    ) {
        next(action)

        when (action) {
            MenuAction.AddBookmark,
            MenuAction.Navigate.EditBookmark,
            -> Events.browserMenuAction.record(
                Events.BrowserMenuActionExtra(
                    item = "bookmark",
                ),
            )

            MenuAction.Navigate.Bookmarks -> Events.browserMenuAction.record(
                Events.BrowserMenuActionExtra(
                    item = "bookmarks",
                ),
            )

            MenuAction.Navigate.CustomizeHomepage -> {
                AppMenu.customizeHomepage.record(NoExtras())
                HomeScreen.customizeHomeClicked.record(NoExtras())
            }

            MenuAction.Navigate.Downloads -> Events.browserMenuAction.record(
                Events.BrowserMenuActionExtra(
                    item = "downloads",
                ),
            )

            MenuAction.Navigate.Help -> HomeMenu.helpTapped.record(NoExtras())

            MenuAction.Navigate.History -> Events.browserMenuAction.record(
                Events.BrowserMenuActionExtra(
                    item = "history",
                ),
            )

            MenuAction.Navigate.ManageExtensions -> Events.browserMenuAction.record(
                Events.BrowserMenuActionExtra(
                    item = "addons_manager",
                ),
            )

            is MenuAction.Navigate.MozillaAccount -> {
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra(item = "sync_account"))
                AppMenu.signIntoSync.add()
            }

            MenuAction.Navigate.NewTab -> Events.browserMenuAction.record(
                Events.BrowserMenuActionExtra(
                    item = "new_tab",
                ),
            )

            MenuAction.Navigate.Passwords -> Events.browserMenuAction.record(
                Events.BrowserMenuActionExtra(
                    item = "passwords",
                ),
            )

            MenuAction.Navigate.ReleaseNotes -> Events.whatsNewTapped.record(NoExtras())

            MenuAction.Navigate.Settings -> {
                when (accessPoint) {
                    MenuAccessPoint.Browser -> Events.browserMenuAction.record(
                        Events.BrowserMenuActionExtra(
                            item = "settings",
                        ),
                    )

                    MenuAccessPoint.Home -> HomeMenu.settingsItemClicked.record(NoExtras())

                    MenuAccessPoint.External -> Unit
                }
            }

            is MenuAction.Navigate.SaveToCollection -> Events.browserMenuAction.record(
                Events.BrowserMenuActionExtra(
                    item = "save_to_collection",
                ),
            )

            MenuAction.Navigate.Share -> Events.browserMenuAction.record(
                Events.BrowserMenuActionExtra(
                    item = "share",
                ),
            )

            MenuAction.Navigate.Translate -> {
                Translations.action.record(Translations.ActionExtra(item = "main_flow_browser"))

                Events.browserMenuAction.record(
                    Events.BrowserMenuActionExtra(
                        item = "translate",
                    ),
                )
            }

            MenuAction.DeleteBrowsingDataAndQuit -> Events.browserMenuAction.record(
                Events.BrowserMenuActionExtra(
                    item = "quit",
                ),
            )

            MenuAction.InitAction,
            MenuAction.Navigate.Back,
            MenuAction.Navigate.DiscoverMoreExtensions,
            MenuAction.Navigate.Extensions,
            MenuAction.Navigate.NewPrivateTab,
            MenuAction.Navigate.Save,
            MenuAction.Navigate.Tools,
            is MenuAction.UpdateBookmarkState,
            -> Unit
        }
    }
}
