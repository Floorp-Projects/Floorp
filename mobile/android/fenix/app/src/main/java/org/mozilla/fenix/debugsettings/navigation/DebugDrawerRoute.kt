/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings.navigation

import androidx.annotation.StringRes
import org.mozilla.fenix.R
import org.mozilla.fenix.debugsettings.ui.DebugDrawerHome
import org.mozilla.fenix.debugsettings.tabs.TabTools as TabToolsScreen

/**
 * The navigation routes for screens within the Debug Drawer.
 *
 * @property route The navigation route of the destination.
 * @property title The string ID of the destination's title.
 */
enum class DebugDrawerRoute(val route: String, @StringRes val title: Int) {
    /**
     * The navigation route for [DebugDrawerHome].
     */
    Home(
        route = "home",
        title = R.string.debug_drawer_title,
    ),

    /**
     * The navigation route for [TabToolsScreen].
     */
    TabTools(
        route = "tab_tools",
        title = R.string.debug_drawer_tab_tools_title,
    ),
}

/**
 * Creates a list of [DebugDrawerDestination]s for the [DebugDrawerRoute]s in Fenix.
 */
fun debugDrawerDestinationsFromRoutes(
    // Composable content dependencies will go here (e.g. browser store, the list of menu items, etc.)
): List<DebugDrawerDestination> = DebugDrawerRoute.values().map { debugDrawerRoute ->
    DebugDrawerDestination(
        route = debugDrawerRoute.route,
        title = debugDrawerRoute.title,
        content = {
            when (debugDrawerRoute) {
                DebugDrawerRoute.Home -> {}
                DebugDrawerRoute.TabTools -> {}
            }
        },
    )
}
