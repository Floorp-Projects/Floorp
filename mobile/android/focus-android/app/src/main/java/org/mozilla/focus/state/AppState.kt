/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.state

import mozilla.components.lib.state.State
import java.util.UUID

/**
 * Global state of the application.
 *
 * @property screen The currently displayed screen.
 */
data class AppState(
    val screen: Screen
) : State

/**
 * A group of screens that the app can display.
 *
 * @property id A unique ID for identifying a screen. Only if this ID changes the screen is
 * considered a new screen that requires a navigation (as opposed to the state of the screen
 * changing).
 */
sealed class Screen {
    open val id = UUID.randomUUID().toString()

    /**
     * First run onboarding.
     */
    object FirstRun : Screen()

    /**
     * The home screen.
     */
    object Home : Screen()

    /**
     * Browser screen.
     *
     * @property tabId The ID of the displayed tab.
     * @property showTabs Whether to show the tabs tray.
     */
    data class Browser(
        val tabId: String,
        val showTabs: Boolean
    ) : Screen() {
        // Whenever the showTabs property changes we want to treat this as a new screen and force
        // a navigation.
        override val id: String = "${super.id}_$showTabs}"
    }

    /**
     * Editing the URL of a tab.
     */
    data class EditUrl(
        val tabId: String,
        val x: Int,
        val y: Int,
        val width: Int,
        val height: Int
    ) : Screen()

    /**
     * The application is locked (and requires unlocking).
     */
    object Locked : Screen()

    /**
     * Tabs have crashed and the user needs to decide whether to restore or close them.
     */
    object Crashed : Screen()
}
