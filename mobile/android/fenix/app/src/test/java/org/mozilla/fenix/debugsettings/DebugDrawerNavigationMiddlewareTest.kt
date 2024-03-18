/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings

import androidx.navigation.NavHostController
import io.mockk.called
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.debugsettings.navigation.DebugDrawerRoute
import org.mozilla.fenix.debugsettings.store.DebugDrawerAction
import org.mozilla.fenix.debugsettings.store.DebugDrawerNavigationMiddleware
import org.mozilla.fenix.debugsettings.store.DebugDrawerStore
import org.mozilla.fenix.debugsettings.ui.DEBUG_DRAWER_HOME_ROUTE

class DebugDrawerNavigationMiddlewareTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val testCoroutineScope = coroutinesTestRule.scope

    private val navController: NavHostController = mockk(relaxed = true)
    private lateinit var store: DebugDrawerStore

    @Before
    fun setup() {
        store = DebugDrawerStore(
            middlewares = listOf(
                DebugDrawerNavigationMiddleware(
                    navController = navController,
                    scope = testCoroutineScope,
                ),
            ),
        )
    }

    @Test
    fun `WHEN home is the next destination THEN the back stack is cleared and the user is returned to home`() {
        store.dispatch(DebugDrawerAction.NavigateTo.Home).joinBlocking()

        verify { navController.popBackStack(route = DEBUG_DRAWER_HOME_ROUTE, inclusive = false) }
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
