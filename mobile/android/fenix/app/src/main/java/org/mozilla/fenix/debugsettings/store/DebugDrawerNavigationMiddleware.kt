/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings.store

import androidx.navigation.NavHostController
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.fenix.debugsettings.navigation.DebugDrawerRoute
import org.mozilla.fenix.debugsettings.ui.DEBUG_DRAWER_HOME_ROUTE

/**
 * Middleware that handles navigation events for the Debug Drawer feature.
 *
 * @param navController [NavHostController] used to execute any navigation actions on the UI.
 * @param scope [CoroutineScope] used to make calls to the main thread.
 */
class DebugDrawerNavigationMiddleware(
    private val navController: NavHostController,
    private val scope: CoroutineScope,
) : Middleware<DebugDrawerState, DebugDrawerAction> {

    override fun invoke(
        context: MiddlewareContext<DebugDrawerState, DebugDrawerAction>,
        next: (DebugDrawerAction) -> Unit,
        action: DebugDrawerAction,
    ) {
        next(action)
        scope.launch {
            when (action) {
                is DebugDrawerAction.NavigateTo.Home -> navController.popBackStack(
                    route = DEBUG_DRAWER_HOME_ROUTE,
                    inclusive = false,
                )
                is DebugDrawerAction.NavigateTo.TabTools ->
                    navController.navigate(route = DebugDrawerRoute.TabTools.route)
                is DebugDrawerAction.OnBackPressed -> navController.popBackStack()
                is DebugDrawerAction.DrawerOpened, DebugDrawerAction.DrawerClosed -> Unit // no-op
            }
        }
    }
}
