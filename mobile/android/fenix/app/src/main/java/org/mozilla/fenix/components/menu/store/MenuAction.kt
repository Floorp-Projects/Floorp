/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.store

import mozilla.components.lib.state.Action

/**
 * Actions to dispatch through the [MenuStore] to modify the [MenuState].
 */
sealed class MenuAction : Action {

    /**
     * Updates whether or not the current selected tab is bookmarked.
     *
     * @property isBookmarked Whether or not the current selected is bookmarked.
     */
    data class UpdateBookmarked(val isBookmarked: Boolean) : MenuAction()

    /**
     * [MenuAction] dispatched when a navigation event occurs for a specific destination.
     */
    sealed class Navigate : MenuAction() {

        /**
         * [Navigate] action dispatched when navigating to the help SUMO article.
         */
        data object Help : Navigate()

        /**
         * [Navigate] action dispatched when navigating to the settings.
         */
        data object Settings : Navigate()
    }
}
