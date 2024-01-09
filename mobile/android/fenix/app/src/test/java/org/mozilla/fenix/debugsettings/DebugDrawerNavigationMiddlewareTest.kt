/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings

import androidx.navigation.NavHostController
import io.mockk.called
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.support.test.ext.joinBlocking
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.debugsettings.navigation.DebugDrawerRoute
import org.mozilla.fenix.debugsettings.store.DebugDrawerAction
import org.mozilla.fenix.debugsettings.store.DebugDrawerNavigationMiddleware
import org.mozilla.fenix.debugsettings.store.DebugDrawerStore

class DebugDrawerNavigationMiddlewareTest {

    private val navController: NavHostController = mockk(relaxed = true)
    private lateinit var store: DebugDrawerStore

    @Before
    fun setup() {
        store = DebugDrawerStore(
            middlewares = listOf(
                DebugDrawerNavigationMiddleware(
                    navController = navController,
                ),
            ),
        )
    }

    @Test
    fun `WHEN home is the next destination THEN home is navigated to`() {
        store.dispatch(DebugDrawerAction.NavigateTo.Home).joinBlocking()

        verify { navController.navigate(DebugDrawerRoute.Home.route) }
    }

    @Test
    fun `WHEN the tab tools screen is the next destination THEN the tab tools screen is navigated to`() {
        store.dispatch(DebugDrawerAction.NavigateTo.TabTools).joinBlocking()

        verify { navController.navigate(DebugDrawerRoute.TabTools.route) }
    }

    @Test
    fun `WHEN the back button is pressed THEN the drawer should go back one screen`() {
        store.dispatch(DebugDrawerAction.OnBackPressed).joinBlocking()

        verify { navController.popBackStack() }
    }

    @Test
    fun `WHEN a non-navigation action is dispatched THEN the drawer should not navigate`() {
        store.dispatch(DebugDrawerAction.DrawerOpened).joinBlocking()
        store.dispatch(DebugDrawerAction.DrawerClosed).joinBlocking()

        verify { navController wasNot called }
    }
}
