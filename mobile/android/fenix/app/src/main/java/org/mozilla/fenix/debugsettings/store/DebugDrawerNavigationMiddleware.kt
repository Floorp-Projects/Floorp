/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings.store

import androidx.navigation.NavHostController
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.fenix.debugsettings.navigation.DebugDrawerRoute

/**
 * Middleware that handles navigation events for the Debug Drawer feature.
 *
 * @param navController [NavHostController] used to execute any navigation actions on the UI.
 */
class DebugDrawerNavigationMiddleware(
    private val navController: NavHostController,
) : Middleware<DebugDrawerState, DebugDrawerAction> {

    override fun invoke(
        context: MiddlewareContext<DebugDrawerState, DebugDrawerAction>,
        next: (DebugDrawerAction) -> Unit,
        action: DebugDrawerAction,
    ) {
        when (action) {
            is DebugDrawerAction.NavigateTo.Home -> navController.navigate(route = DebugDrawerRoute.Home.route)
            is DebugDrawerAction.NavigateTo.TabTools -> navController.navigate(route = DebugDrawerRoute.TabTools.route)
            is DebugDrawerAction.OnBackPressed -> navController.popBackStack()
            is DebugDrawerAction.DrawerOpened, DebugDrawerAction.DrawerClosed -> {} // no-op
        }
    }
}
