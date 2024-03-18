/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings.store

import mozilla.components.lib.state.Action
import org.mozilla.fenix.debugsettings.ui.DebugDrawerHome
import org.mozilla.fenix.debugsettings.tabs.TabTools as TabToolsScreen

/**
 * [Action] implementation related to [DebugDrawerStore].
 */
sealed class DebugDrawerAction : Action {

    /**
     * [DebugDrawerAction] fired when the user opens the drawer.
     */
    object DrawerOpened : DebugDrawerAction()

    /**
     * [DebugDrawerAction] fired when the user closes the drawer.
     */
    object DrawerClosed : DebugDrawerAction()

    /**
     * [DebugDrawerAction] fired when a navigation event occurs for a specific destination.
     */
    sealed class NavigateTo : DebugDrawerAction() {

        /**
         * [NavigateTo] action fired when the debug drawer needs to navigate to [DebugDrawerHome].
         */
        object Home : NavigateTo()

        /**
         * [NavigateTo] action fired when the debug drawer needs to navigate to [TabToolsScreen].
         */
        object TabTools : NavigateTo()
    }

    /**
     * [DebugDrawerAction] fired when a back navigation event occurs.
     */
    object OnBackPressed : DebugDrawerAction()
}
