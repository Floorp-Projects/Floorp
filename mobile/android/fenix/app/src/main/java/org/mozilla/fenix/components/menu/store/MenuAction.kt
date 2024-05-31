/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.store

import mozilla.components.lib.state.Action
import mozilla.components.service.fxa.manager.AccountState
import org.mozilla.fenix.components.menu.MenuAccessPoint

/**
 * Actions to dispatch through the [MenuStore] to modify the [MenuState].
 */
sealed class MenuAction : Action {

    /**
     * [MenuAction] dispatched to indicate that the store is initialized and
     * ready to use. This action is dispatched automatically before any other
     * action is processed. Its main purpose is to trigger initialization logic
     * in middlewares.
     */
    data object InitAction : MenuAction()

    /**
     * [MenuAction] dispatched when a bookmark is to be added.
     */
    data object AddBookmark : MenuAction()

    /**
     * [MenuAction] dispatched when a bookmark state is updated.
     *
     * @property bookmarkState The new [BookmarkState] to be updated.
     */
    data class UpdateBookmarkState(val bookmarkState: BookmarkState) : MenuAction()

    /**
     * [MenuAction] dispatched to delete browsing data and quit the browser.
     */
    data object DeleteBrowsingDataAndQuit : MenuAction()

    /**
     * [MenuAction] dispatched when a navigation event occurs for a specific destination.
     */
    sealed class Navigate : MenuAction() {

        /**
         * [Navigate] action dispatched when navigating to Mozilla account.
         *
         * @property accountState The [AccountState] of a Mozilla account.
         * @property accesspoint The access point that was used to navigate to the menu.
         */
        data class MozillaAccount(
            val accountState: AccountState,
            val accesspoint: MenuAccessPoint,
        ) : Navigate()

        /**
         * [Navigate] action dispatched when navigating to the help SUMO article.
         */
        data object Help : Navigate()

        /**
         * [Navigate] action dispatched when navigating to the settings.
         */
        data object Settings : Navigate()

        /**
         * [Navigate] action dispatched when navigating to bookmarks.
         */
        data object Bookmarks : Navigate()

        /**
         * [Navigate] action dispatched when navigating to history.
         */
        data object History : Navigate()

        /**
         * [Navigate] action dispatched when navigating to downloads.
         */
        data object Downloads : Navigate()

        /**
         * [Navigate] action dispatched when navigating to passwords.
         */
        data object Passwords : Navigate()

        /**
         * [Navigate] action dispatched when navigating to customize homepage.
         */
        data object CustomizeHomepage : Navigate()

        /**
         * [Navigate] action dispatched when navigating to release notes.
         */
        data object ReleaseNotes : Navigate()

        /**
         * [Navigate] action dispatched when navigating to the tools submenu.
         */
        data object Tools : Navigate()

        /**
         * [Navigate] action dispatched when navigating to the save submenu.
         */
        data object Save : Navigate()

        /**
         * [Navigate] action dispatched when navigating to the extensions submenu.
         */
        data object Extensions : Navigate()

        /**
         * [Navigate] action dispatched when a back navigation event occurs.
         */
        data object Back : Navigate()

        /**
         * [Navigate] action dispatched when navigating to edit the existing bookmark.
         */
        data object EditBookmark : Navigate()

        /**
         * [Navigate] action dispatched when navigating to save a site to a collection.
         *
         * @property hasCollection Whether or not there are any existing tab collections.
         */
        data class SaveToCollection(
            val hasCollection: Boolean,
        ) : Navigate()

        /**
         * [Navigate] action dispatched when navigating to translations dialog.
         */
        data object Translate : Navigate()

        /**
         * [Navigate] action dispatched when navigating to the share sheet.
         */
        data object Share : Navigate()

        /**
         * [Navigate] action dispatched when navigating to the extensions manager.
         */
        data object ManageExtensions : Navigate()

        /**
         * [Navigate] action dispatched when navigating to the AMO page.
         */
        data object DiscoverMoreExtensions : Navigate()

        /**
         * [Navigate] action dispatched when navigating to the new tab.
         */
        data object NewTab : Navigate()

        /**
         * [Navigate] action dispatched when navigating to the new private tab.
         */
        data object NewPrivateTab : Navigate()
    }
}
